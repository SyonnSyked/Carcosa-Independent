#include "GameManager.h"
#include "../DRAW/DrawComponents.h"
#include "../UTIL/Utilities.h"
#include "GameComponents.h"
#include "GameplaySystems.h"
#include "imgui.h"

namespace GAME {
GameState CurrentState;
ImGuiStyle buttonStyle;
ButtonDim buttonDiemension;

namespace {
void AttachCharacter(entt::registry &registry, entt::entity entity, const CharacterConfig &config)
{
    registry.emplace<Health>(entity, Health{config.health, config.health});
    registry.emplace<Movement>(entity, Movement{config.speed});
    auto attack = config.attackData;
    attack.remaining = 0;
    registry.emplace<MeleeAttack>(entity, attack);
}

CharacterConfig LoadCharacter(entt::registry &registry, const char *role)
{
    const auto &settings = *registry.ctx().get<UTIL::Config>().gameConfig;
    const auto &tuning = settings.at(role);
    CharacterConfig config;
    config.model = tuning.at("model").as<std::string>();
    config.health = settings.RequireFloat(role, "health");
    config.speed = settings.RequireFloat(role, "speed");
    config.attackData = {settings.RequireFloat(role, "damage"),
                         settings.RequireFloat(role, "range"),
                         settings.RequireFloat(role, "cooldown")};
    if (!std::isfinite(config.health) || config.health <= 0 || !std::isfinite(config.speed) ||
        config.speed < 0 || !std::isfinite(config.attackData.damage) ||
        config.attackData.damage < 0 || !std::isfinite(config.attackData.range) ||
        config.attackData.range < 0 || !std::isfinite(config.attackData.cooldown) ||
        config.attackData.cooldown < 0)
        throw std::runtime_error(std::string("Invalid character tuning: ") + role);
    const auto *manager = registry.ctx().find<DRAW::ModelManager>();
    if (!manager || !manager->models.contains(config.model) ||
        manager->models.at(config.model).meshEntities.empty())
        throw std::runtime_error("Gameplay renderable template not loaded: " + config.model);
    const auto levels = registry.view<DRAW::CPULevel>();
    if (!levels.empty()) {
        const auto &level = registry.get<DRAW::CPULevel>(levels.front()).lvlData;
        for (const auto &object : level.blenderObjects) {
            if (config.model == object.blendername) {
                config.transform = {level.levelTransforms.at(object.transformIndex)};
                return config;
            }
        }
    }
    throw std::runtime_error("Gameplay object not loaded: " + config.model);
}

HordeSpawner LoadHordeSpawner(const GameConfig &settings)
{
    const auto &tuning = settings.at("HordeSpawner");
    HordeSpawner spawner{tuning.at("enabled").as<bool>(),
                         settings.RequireFloat("HordeSpawner", "spawnInterval"),
                         settings.RequireFloat("HordeSpawner", "minimumRadius"),
                         settings.RequireFloat("HordeSpawner", "maximumRadius"),
                         tuning.at("maximumAlive").as<int>(),
                         tuning.at("totalSpawnLimit").as<int>()};
    if (!std::isfinite(spawner.spawnInterval) || spawner.spawnInterval < 0 ||
        !std::isfinite(spawner.minimumRadius) || spawner.minimumRadius < 0 ||
        !std::isfinite(spawner.maximumRadius) || spawner.minimumRadius > spawner.maximumRadius ||
        spawner.maximumAlive <= 0 || spawner.totalSpawnLimit < 0)
        throw std::runtime_error(
            "Invalid HordeSpawner configuration: check interval, radii and population limits");
    return spawner;
}

void InitializeGameplaySettings(entt::registry &registry, const GameConfig &settings)
{
    GameplayCameraConfig cameraConfig{{settings.RequireFloat("GameplayCamera", "x"),
                                       settings.RequireFloat("GameplayCamera", "y"),
                                       settings.RequireFloat("GameplayCamera", "z"), 0}};
    if (!std::isfinite(cameraConfig.offset.x) || !std::isfinite(cameraConfig.offset.y) ||
        !std::isfinite(cameraConfig.offset.z))
        throw std::runtime_error("Invalid GameplayCamera configuration: offset must be finite");

    PickupConfig pickupConfig{settings.RequireFloat("Pickup", "collectionAllowance")};
    if (!std::isfinite(pickupConfig.collectionAllowance) || pickupConfig.collectionAllowance < 0)
        throw std::runtime_error(
            "Invalid Pickup configuration: collectionAllowance must be finite and nonnegative");

    registry.ctx().emplace<GameplayCameraConfig>(cameraConfig);
    registry.ctx().emplace<PickupConfig>(pickupConfig);
}

void IntegrateVelocity(entt::registry &registry, float deltaTime)
{
    for (auto entity : registry.view<Transform, Velocity>()) {
        auto &transform = registry.get<Transform>(entity).matrix;
        const auto &velocity = registry.get<Velocity>(entity).vector;
        transform.row4.x += velocity.x * deltaTime;
        transform.row4.y += velocity.y * deltaTime;
        transform.row4.z += velocity.z * deltaTime;
    }
}

void SynchronizeMeshTransforms(entt::registry &registry)
{
    for (auto entity : registry.view<Transform, DRAW::MeshCollection>()) {
        const auto &transform = registry.get<Transform>(entity).matrix;
        const auto &meshes = registry.get<DRAW::MeshCollection>(entity).meshEntities;
        for (auto mesh : meshes)
            if (registry.valid(mesh) && registry.all_of<DRAW::GPUInstance>(mesh))
                registry.get<DRAW::GPUInstance>(mesh).transform = transform;
    }
}

void DestroyPendingEntities(entt::registry &registry)
{
    const auto pending = registry.view<ToDestroy>();
    std::vector<entt::entity> entities(pending.begin(), pending.end());
    for (auto entity : entities)
        if (registry.valid(entity))
            registry.destroy(entity);
}

} // namespace

entt::entity CreateRenderable(entt::registry &registry, const std::string &modelName,
                              const GW::MATH::GMATRIXF &transform)
{
    const auto *manager = registry.ctx().find<DRAW::ModelManager>();
    if (!manager || !manager->models.contains(modelName))
        throw std::runtime_error("Renderable template not loaded: " + modelName +
                                 " (export it in the selected world with gameType != none)");
    const auto &source = manager->models.at(modelName);
    for (auto mesh : source.meshEntities)
        if (!registry.valid(mesh) || !registry.all_of<DRAW::GeometryData, DRAW::GPUInstance>(mesh))
            throw std::runtime_error("Renderable template is no longer valid: " + modelName);

    const auto entity = registry.create();
    try {
        registry.emplace<Transform>(entity, Transform{transform});
        registry.emplace<DRAW::MeshCollection>(entity, DRAW::MeshCollection{{}, source.collider});
        for (auto mesh : source.meshEntities) {
            const auto geometry = registry.get<DRAW::GeometryData>(mesh);
            auto instance = registry.get<DRAW::GPUInstance>(mesh);
            instance.transform = transform;
            const auto clone = registry.create();
            registry.get<DRAW::MeshCollection>(entity).meshEntities.push_back(clone);
            registry.emplace<DRAW::GeometryData>(clone, geometry);
            registry.emplace<DRAW::GPUInstance>(clone, instance);
        }
    } catch (...) {
        registry.destroy(entity);
        throw;
    }
    return entity;
}

void InitializeGameplay(entt::registry &registry)
{
    const auto &settings = *registry.ctx().get<UTIL::Config>().gameConfig;
    const auto playerConfig = LoadCharacter(registry, "Player");
    const auto enemyConfig = LoadCharacter(registry, "Enemy");
    const auto spawner = LoadHordeSpawner(settings);
    InitializeGameplaySettings(registry, settings);

    // The shared recipe is parsed once. Each actor still owns independent ECS state.
    InitializeDamageFeedback(registry);
    registry.ctx().emplace<CharacterConfig>(enemyConfig);
    const auto player =
        CreateRenderable(registry, playerConfig.model, playerConfig.transform.matrix);
    registry.emplace<Player>(player);
    registry.emplace<Class>(player, Class{"Warrior"});
    registry.emplace<WarriorAOE>(player);
    AttachCharacter(registry, player, playerConfig);
    CreateEnemy(registry, enemyConfig, enemyConfig.transform.matrix);
    // Reuse the loaded enemy model; the orange tint identifies the ranged variant.
    auto rangedPlacement = playerConfig.transform.matrix;
    rangedPlacement.row4.x += 5.0f;
    rangedPlacement.row4.z += 3.0f;
    const auto rangedEnemy = CreateEnemy(registry, enemyConfig, rangedPlacement);
    registry.emplace<RangedAttack>(rangedEnemy);
    for (auto mesh : registry.get<DRAW::MeshCollection>(rangedEnemy).meshEntities) {
        auto &material = registry.get<DRAW::GPUInstance>(mesh).matData;
        material.Kd = {1.0f, 0.35f, 0.05f};
    }
    auto wizardPlacement = playerConfig.transform.matrix;
    wizardPlacement.row4.x -= 5.0f;
    wizardPlacement.row4.z += 3.0f;
    const auto wizard = CreateEnemy(registry, enemyConfig, wizardPlacement);
    registry.emplace<WizardAttack>(wizard);
    for (auto mesh : registry.get<DRAW::MeshCollection>(wizard).meshEntities)
        registry.get<DRAW::GPUInstance>(mesh).matData.Kd = {0.65f, 0.1f, 1.0f};
    const auto spawnerEntity = registry.create();
    registry.emplace<HordeSpawner>(spawnerEntity, spawner);
    registry.emplace<Transform>(spawnerEntity, enemyConfig.transform);
    UpdateCamera(registry);
    std::cout << "Gameplay ready: WASD moves, left-click an enemy in melee range to attack."
              << std::endl;
}

void ButtonStyleSetup()
{
    buttonStyle = ImGui::GetStyle();
    buttonStyle.Colors[ImGuiCol_Button] = ImVec4(1.0f,1.0f,1.0f,1.0f);
    buttonStyle.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f,0.0f,0.0f,1.0f);
    buttonStyle.Colors[ImGuiCol_ButtonActive] = ImVec4(0.0f,1.0f,0.0f,1.0f);
}

void RestartGame(entt::registry &registry) // needs to destroy everything and then call init
{
    // Keep window, renderer, level geometry and hidden model templates alive.
    std::vector<entt::entity> gameplayEntities;
    auto collect = [&](auto view) {
        for (auto entity : view)
            gameplayEntities.push_back(entity);
    };
    collect(registry.view<Player>());
    collect(registry.view<Enemy>());
    collect(registry.view<Projectile>());
    collect(registry.view<SpellArea>());
    collect(registry.view<HordeSpawner>());
    for (auto entity : gameplayEntities)
        if (registry.valid(entity))
            registry.destroy(entity); // MeshCollection owns each actor's visible meshes.
    InitializeGameplay(registry);
    ChangeScreen(Screens::Playing);
}




void UpdateGameSystems(entt::registry &registry)
{
    static bool prevEscaped = false;
    float state = 0.0f;
    registry.ctx().get<UTIL::Input>().immediateInput.GetState(G_KEY_ESCAPE, state);
    const bool pressed = state > 0.0f && !prevEscaped;
    prevEscaped = state > 0.0f;
    if (pressed) {
        if (CurrentState.ActiveScreen == Screens::Playing)
            ChangeScreen(Screens::PauseMenu);
        else if (CurrentState.ActiveScreen == Screens::PauseMenu)
            ChangeScreen(Screens::Playing);
    }
    if (!CurrentState.Running || CurrentState.ActiveScreen != Screens::Playing)
        return;

    const float deltaTime = static_cast<float>(registry.ctx().get<UTIL::DeltaTime>().dtSec);
    // Age previous flashes first; hits this frame get their full configured duration.
    UpdateDamageFlash(registry, deltaTime);
    UpdateCombatTimers(registry, deltaTime);
    UpdatePlayer(registry, deltaTime);
    // Age existing warnings before creating new ones so each gets its full delay.
    UpdateSpellAreas(registry, deltaTime);
    UpdateEnemy(registry, deltaTime);
    UpdateProjectiles(registry, deltaTime);
    // Combat deaths free population slots now; new enemies begin AI next frame.
    // Spawn before mesh synchronization so new instances render at their placement immediately.
    UpdateHordeSpawner(registry, deltaTime);
    UpdateCamera(registry);
    IntegrateVelocity(registry, deltaTime);
    SynchronizeMeshTransforms(registry);
    DestroyPendingEntities(registry);
}

entt::entity CreateEnemy(entt::registry &registry, const CharacterConfig &config,
                         const GW::MATH::GMATRIXF &placement)
{
    const auto enemy = CreateRenderable(registry, config.model, placement);
    try {
        registry.emplace<Enemy>(enemy);
        AttachCharacter(registry, enemy, config);
    } catch (...) {
        registry.destroy(enemy); // Existing MeshCollection callback owns mesh cleanup.
        throw;
    }
    return enemy;
}

void ChangeScreen(Screens s)
{
    CurrentState.ActiveScreen=s;
}

void RenderScreens(entt::registry &registry)
{
 switch (CurrentState.ActiveScreen)
 {
 case Screens::Playing: // need my man to put the regular rendering stuff in here
    //for if we want a hud and what not
   
   break;
case Screens::MainMenu:
    DrawBlackWindow();
    DrawMainMenu();
    break;
case Screens::PauseMenu:   
    DrawPauseMenu();
    break;
case Screens::GameOverMenu:
    DrawBlackWindow();
    DrawGameOver(registry);
    break;

 }
}

void DrawMainMenu()
{
    ImGuiIO& io = ImGui::GetIO();

    const float spacing = 20.0f;
    const float totatlH = buttonDiemension.height + spacing;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f,io.DisplaySize.y * 0.5f),
                            ImGuiCond_Always,ImVec2(0.5f,0.5f));
    
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.66f,io.DisplaySize.y),ImGuiCond_Always);                      
   
    ImGui::Begin("##mainmenu",nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse);

    ImVec2 winPos  = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();

    float startX = winPos.x + (winSize.x - buttonDiemension.width) * 0.5;
    float startY = winPos.y + (winSize.y - totatlH) * 0.5f;

    ImGui::SetCursorScreenPos(ImVec2(startX,startY));
    if(ImGui::Button("Start",ImVec2(buttonDiemension.width,buttonDiemension.height)))
        ChangeScreen(Screens::Playing);

    ImGui::SetCursorScreenPos(ImVec2(startX,startY + buttonDiemension.height + spacing));
    if(ImGui::Button("Quit",ImVec2(buttonDiemension.width,buttonDiemension.height)))
        CurrentState.Running = false;
        

    ImGui::End();
}


void DrawPauseMenu()
{
    ImGuiIO& io = ImGui::GetIO();

    const float spacing = 20.0f;
    const float totatlH = buttonDiemension.height + spacing;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f,io.DisplaySize.y * 0.5f));

    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.66f,io.DisplaySize.y),ImGuiCond_Always);                      
   
    ImGui::Begin("##pausemenu",nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse);

    ImVec2 winPos  = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();

    float startX = winPos.x + (winSize.x - buttonDiemension.width) * 0.5;
    float startY = winPos.y + (winSize.y - totatlH) * 0.5f;


    ImGui::SetCursorScreenPos(ImVec2(startX,startY));
    if(ImGui::Button("Resume",ImVec2(buttonDiemension.width,buttonDiemension.height)))
        ChangeScreen(Screens::Playing);

    ImGui::SetCursorScreenPos(ImVec2(startX,startY + buttonDiemension.height + spacing));
    if(ImGui::Button("Main Menu",ImVec2(buttonDiemension.width,buttonDiemension.height)))
        ChangeScreen(Screens::MainMenu);
    
    ImGui::SetCursorScreenPos(ImVec2(startX,startY + 2*(buttonDiemension.height  + spacing)));
    if(ImGui::Button("Quit",ImVec2(buttonDiemension.width,buttonDiemension.height)))
        CurrentState.Running = false;

    ImGui::End();

}


void DrawGameOver(entt::registry& reg)
{
    ImGuiIO& io = ImGui::GetIO();

    const float spacing = 20.0f;
    const float totatlH = buttonDiemension.height + spacing;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f,io.DisplaySize.y * 0.5f),
                            ImGuiCond_Always,ImVec2(0.5f,0.5f));
    
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.66f,io.DisplaySize.y),ImGuiCond_Always);                      
   
    ImGui::Begin("##gameover",nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse);

    ImVec2 winPos  = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();

    float startX = winPos.x + (winSize.x - buttonDiemension.width) * 0.5;
    float startY = winPos.y + (winSize.y - totatlH) * 0.5f;

    ImGui::SetCursorScreenPos(ImVec2(startX,startY));
    if(ImGui::Button("Main Menu",ImVec2(buttonDiemension.width,buttonDiemension.height)))
        ChangeScreen(Screens::MainMenu);

    ImGui::SetCursorScreenPos(ImVec2(startX,startY + buttonDiemension.height + spacing));
    if(ImGui::Button("Restart",ImVec2(buttonDiemension.width,buttonDiemension.height)))
        RestartGame(reg);
        

    ImGui::End();
}

void DrawBlackWindow()
{
    ImGuiIO& io = ImGui::GetIO();

    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0),ImVec2(io.DisplaySize.x, io.DisplaySize.y),IM_COL32(0, 0, 0, 255));
}


} // namespace GAME
