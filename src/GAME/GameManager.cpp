#include "GameManager.h"
#include "../DRAW/DrawComponents.h"
#include "../UTIL/Utilities.h"
#include "GameComponents.h"
#include "GameplaySystems.h"
#include "imgui.h"
#include <cstdio>
#include <sstream>

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
    //    auto abilities = config.characterClass;
    //   registry.emplace<Abilities>(entity, abilities);
}

CharacterConfig LoadCharacter(entt::registry &registry, const char *role)
{
    const auto &settings = *registry.ctx().get<UTIL::Config>().gameConfig;
    const auto &tuning = settings.at(role);
    CharacterConfig config;
    config.characterClass.Type = role;
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

VampiricDrainAttack LoadVampiricDrain(const GameConfig &settings)
{
    VampiricDrainAttack drain{settings.RequireFloat("VampireQueen", "drainDamage"),
                              settings.RequireFloat("VampireQueen", "drainLifeSteal"),
                              settings.RequireFloat("VampireQueen", "drainRange"),
                              settings.RequireFloat("VampireQueen", "drainCooldown")};
    if (drain.damage < 0 || drain.lifeSteal < 0 || drain.range < 0 || drain.cooldown < 0)
        throw std::runtime_error("Invalid VampireQueen drain tuning: values must be nonnegative");
    return drain; // RequireFloat already rejects nonfinite values.
}
     
SpawnSquiresAttack LoadSquireSpawn(const GameConfig &settings)
{
    
    SpawnSquiresAttack spawn;
    spawn.SquireMin = settings.RequireFloat("UndeadKnight", "squireMin");
    spawn.SquireMax = settings.RequireFloat("UndeadKnight", "squireMax");
    spawn.CoolDown =  settings.RequireFloat("UndeadKnight", "summonCD");
    spawn.Radius = settings.RequireFloat("UndeadKnight", "spawnRadius");
    
    if(spawn.SquireMin < 0 || spawn.SquireMax < 0 || spawn.CoolDown < 0 || spawn.Radius < 0)
        throw std::runtime_error("Knight has improper values");
    return spawn;      
}

RingAttackStats LoadKnightRingAttack(const GameConfig &settings)
{
    RingAttackStats Ring;

    Ring.CoolDown = settings.RequireFloat("UndeadKnight","CoolDown");
    Ring.Damage = settings.RequireFloat("UndeadKnight", "ringDamage");
    Ring.WaitTime = settings.RequireFloat("UndeadKnight", "WaitTime");

    Ring.Rings[0] = ReturnRingRadius(settings,"UndeadKnight", "ring1");
    Ring.Rings[1] = ReturnRingRadius(settings, "UndeadKnight", "ring2");
    Ring.Rings[3] = ReturnRingRadius(settings, "UndeadKnight", "ring3");

    Ring.state = RingState::Idle;
    Ring.CoolDown = 0;

    return Ring;

}

Ring ReturnRingRadius(const GameConfig& settings, const std::string& section, const std::string& key)
{
    std::string raw = settings.at(section).at(key).as<std::string>();

    for(char& c : raw)
        {
            if (c == ',') 
                c = ' ';
        }


    std::stringstream ss(raw);
    Ring ring;

    ss >> ring.Inner >> ring.Outter;

    return ring;
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
    // Older configurations retain equal weights; present values use strict float parsing.
    auto weight = [&](const char *key) {
        return tuning.find(key) == tuning.end() ? 1.0f : settings.RequireFloat("HordeSpawner", key);
    };
    spawner.meleeWeight = weight("meleeWeight");
    spawner.rangedWeight = weight("rangedWeight");
    spawner.wizardWeight = weight("wizardWeight");
    if (spawner.meleeWeight < 0 || spawner.rangedWeight < 0 || spawner.wizardWeight < 0 ||
        (spawner.meleeWeight == 0 && spawner.rangedWeight == 0 && spawner.wizardWeight == 0))
        throw std::runtime_error("Invalid HordeSpawner weights: require nonnegative values and at "
                                 "least one positive weight");
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
    CollisionConfig collision{settings.RequireFloat("Collision", "playerRadius")};
    if (collision.playerRadius <= 0)
        throw std::runtime_error("Invalid Collision.playerRadius: must be positive and finite");
    registry.ctx().emplace<CollisionConfig>(collision);
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
    ResetEnemyKillCounter();
    const auto &settings = *registry.ctx().get<UTIL::Config>().gameConfig;
    const auto playerConfig = LoadCharacter(registry, "Player");
    const auto enemyConfig = LoadCharacter(registry, "Enemy");
   
    const auto spawner = LoadHordeSpawner(settings);
    InitializeGameplaySettings(registry, settings);
    InitializeDungeonKeys(registry);

    // The shared recipe is parsed once. Each actor still owns independent ECS state.
    InitializeDamageFeedback(registry);
    registry.ctx().emplace<CharacterConfig>(enemyConfig);
    const auto player =
        CreateRenderable(registry, playerConfig.model, playerConfig.transform.matrix);
    registry.emplace<Player>(player);
    registry.emplace<Class>(player, Class{"Warrior"});
    registry.emplace<WarriorAOE>(player);
    registry.emplace<AxeThrow>(player);
    AttachCharacter(registry, player, playerConfig);

    CreateEnemy(registry, enemyConfig, enemyConfig.transform.matrix);
    CreateEnemy(registry, enemyConfig, enemyConfig.transform.matrix);
    // Reuse the loaded enemy model; the orange tint identifies the ranged variant.
    auto rangedPlacement = playerConfig.transform.matrix;
    rangedPlacement.row4.x += 5.0f;
    rangedPlacement.row4.z += 3.0f;
    CreateEnemyVariant(registry, enemyConfig, rangedPlacement, EnemyVariant::Ranged);
    auto wizardPlacement = playerConfig.transform.matrix;
    wizardPlacement.row4.x -= 5.0f;
    wizardPlacement.row4.z += 3.0f;
    CreateEnemyVariant(registry, enemyConfig, wizardPlacement, EnemyVariant::Wizard);

    const auto drainConfig = LoadVampiricDrain(settings);
    const auto queenModel = settings.at("VampireQueen").at("model").as<std::string>();
    const auto *models = registry.ctx().find<DRAW::ModelManager>();
    if (models && models->models.contains(queenModel) &&
        !models->models.at(queenModel).meshEntities.empty()) {
        const auto queenConfig = LoadCharacter(registry, "VampireQueen");
        CreateVampireQueen(registry, queenConfig, drainConfig, queenConfig.transform.matrix);
    } else {
        std::cout << "Vampire Queen not spawned: missing dynamic Blender template " << queenModel
                  << " in the selected level (export its H2B and object with gameType != none)."
                  << std::endl;
    }

    const auto KnightSpawnConfig = LoadSquireSpawn(settings);
    const auto KnightRingAttack = LoadKnightRingAttack(settings);
    const auto KnightModel = settings.at("UndeadKnight").at("model").as<std::string>();
    const auto *models = registry.ctx().find<DRAW::ModelManager>();
    if(models && models->models.contains(KnightModel) &&

        !models->models.at(KnightModel).meshEntities.empty())
        {
            const auto KnightConfig = LoadCharacter(registry,"UndeadKnight");
            CreatUndeadKnight(registry,KnightConfig,KnightSpawnConfig,KnightConfig.transform.matrix);                

        }

        else{

            std::cout << "Undead Knight not spawned: missing dynamic Blender template " << KnightModel
                     << " in the selected level (export its H2B and object with gameType != none)."
                    << std::endl;

        }



    const auto spawnerEntity = registry.create();
    registry.emplace<HordeSpawner>(spawnerEntity, spawner);
    registry.emplace<Transform>(spawnerEntity, enemyConfig.transform);
    UpdateCamera(registry);
    std::cout << "Gameplay ready: WASD moves, left-click an enemy for melee, hold right-click "
                 "to throw axes toward the cursor, 1 uses Warrior AOE."
              << std::endl;
}

void RestartGame(entt::registry &registry)
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
    collect(registry.view<ThrownAxe>());
    collect(registry.view<SpellArea>());
    collect(registry.view<HordeSpawner>());
    collect(registry.view<DungeonKey>());
    collect(registry.view<DungeonExit>());
    collect(registry.view<PickUp>());
    collect(registry.view<ToDestroy>());
    for (auto entity : gameplayEntities)
        if (registry.valid(entity))
            registry.destroy(entity); // MeshCollection owns each actor's visible meshes.
    registry.ctx().erase<DungeonState>();
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

    if (registry.ctx().get<DungeonState>().complete)
        return;
    const float deltaTime = static_cast<float>(registry.ctx().get<UTIL::DeltaTime>().dtSec);
    // Age previous flashes first; hits this frame get their full configured duration.
    UpdateDamageFlash(registry, deltaTime);
    UpdateCombatTimers(registry, deltaTime);
    UpdatePlayer(registry, deltaTime);
    // Age existing warnings before creating new ones so each gets its full delay.
    UpdateSpellAreas(registry, deltaTime);
    if (CurrentState.ActiveScreen != Screens::Playing) {
        DestroyPendingEntities(registry);
        return;
    }
    UpdateEnemy(registry, deltaTime);
    if (CurrentState.ActiveScreen != Screens::Playing) {
        DestroyPendingEntities(registry);
        return;
    }
    UpdateProjectiles(registry, deltaTime);
    if (CurrentState.ActiveScreen != Screens::Playing) {
        DestroyPendingEntities(registry);
        return;
    }
    UpdateThrownAxes(registry, deltaTime);
    UpdateVampireQueen(registry, deltaTime);
    if (CurrentState.ActiveScreen != Screens::Playing) {
        DestroyPendingEntities(registry);
        return;
    }
    UpdateKnight(registry,deltaTime);
    if (CurrentState.ActiveScreen != Screens::Playing) {
        DestroyPendingEntities(registry);
        return;
    }
    
    
    // Combat deaths free population slots now; new enemies begin AI next frame.
    // Spawn before mesh synchronization so new instances render at their placement immediately.
    UpdateHordeSpawner(registry, deltaTime);
    UpdateCamera(registry);
    IntegrateVelocity(registry, deltaTime);
    CollectDungeonKey(registry);
    UseDungeonKey(registry);
    SynchronizeMeshTransforms(registry);
    DestroyPendingEntities(registry);
}

entt::entity CreateEnemy(entt::registry &registry, const CharacterConfig &config,
                         const GW::MATH::GMATRIXF &placement)
{
    const auto enemy = CreateRenderable(registry, config.model, placement);
    try {
        registry.emplace<Enemy>(enemy, Enemy{false, 0});
        AttachCharacter(registry, enemy, config);
    } catch (...) {
        registry.destroy(enemy); // Existing MeshCollection callback owns mesh cleanup.
        throw;
    }
    return enemy;
}

entt::entity CreateEnemyVariant(entt::registry &registry, const CharacterConfig &config,
                                const GW::MATH::GMATRIXF &placement, EnemyVariant variant)
{
    const auto enemy = CreateEnemy(registry, config, placement);
    try {
        if (variant == EnemyVariant::Ranged)
            registry.emplace<RangedAttack>(enemy);
        else if (variant == EnemyVariant::Wizard)
            registry.emplace<WizardAttack>(enemy);
        if (variant != EnemyVariant::Melee) {
            for (auto mesh : registry.get<DRAW::MeshCollection>(enemy).meshEntities)
                registry.get<DRAW::GPUInstance>(mesh).matData.Kd =
                    variant == EnemyVariant::Ranged ? H2B::VECTOR{1.0f, 0.35f, 0.05f}
                                                    : H2B::VECTOR{0.65f, 0.1f, 1.0f};
        }
    } catch (...) {
        registry.destroy(enemy); // Same owned-mesh cleanup as CreateEnemy.
        throw;
    }
    return enemy;
}

void ChangeScreen(Screens s)
{
    CurrentState.ActiveScreen = s;
}

namespace {
void DrawPlayerHUD(entt::registry &registry)
{
    const auto players = registry.view<Player, Health>(entt::exclude<ToDestroy>);
    if (players.begin() == players.end())
        return;
    const auto player = players.front();
    const auto &health = registry.get<Health>(player);
    const auto *viewport = ImGui::GetMainViewport();
    if (viewport->Size.x <= 0 || viewport->Size.y <= 0)
        return;
    const float scale = (std::min)(1.5f, (std::min)(viewport->Size.x / 1280.0f,
                                                   viewport->Size.y / 720.0f));
    const float left = viewport->Pos.x + (viewport->Size.x - 452 * scale) * 0.5f;
    const float top = viewport->Pos.y + viewport->Size.y - 160 * scale;
    // Draw-list primitives create no widgets/windows that could capture combat input.
    auto *draw = ImGui::GetBackgroundDrawList();
    const ImU32 panel = IM_COL32(16, 18, 17, 235);
    const ImU32 border = IM_COL32(139, 121, 79, 255);
    const ImU32 gold = IM_COL32(222, 195, 129, 255);
    const ImU32 text = IM_COL32(235, 229, 211, 255);
    const ImU32 muted = IM_COL32(157, 157, 146, 255);
    auto point = [&](float x, float y) { return ImVec2(left + x * scale, top + y * scale); };
    auto label = [&](float x, float y, float size, ImU32 color, const char *value) {
        draw->AddText(ImGui::GetFont(), size * scale, point(x, y), color, value);
    };
    auto centered = [&](float x, float y, float width, float size, ImU32 color, const char *value) {
        const float textWidth = ImGui::GetFont()->CalcTextSizeA(size * scale, FLT_MAX, 0, value).x;
        draw->AddText(ImGui::GetFont(), size * scale,
                      ImVec2(left + (x + width * 0.5f) * scale - textWidth * 0.5f,
                             top + y * scale), color, value);
    };

    draw->AddRectFilled(point(0, 0), point(452, 142), panel, 6 * scale);
    draw->AddRect(point(0, 0), point(452, 142), border, 6 * scale);
    label(14, 10, 14, gold, "WARRIOR");
    const float maximum = std::isfinite(health.maximum) ? (std::max)(0.0f, health.maximum) : 0;
    const float current = std::isfinite(health.current) ? (std::clamp)(health.current, 0.0f, maximum) : 0;
    char hp[64];
    std::snprintf(hp, sizeof(hp), "HP  %.0f / %.0f", current, maximum);
    label(270, 10, 14, text, hp);
    draw->AddRectFilled(point(14, 31), point(438, 47), IM_COL32(45, 27, 27, 255), 3 * scale);
    const float fraction = maximum > 0 ? current / maximum : 0;
    if (fraction > 0)
        draw->AddRectFilled(point(14, 31), point(14 + 424 * fraction, 47),
                            fraction <= 0.25f ? IM_COL32(225, 63, 53, 255) : IM_COL32(171, 49, 48, 255),
                            3 * scale);

    auto slot = [&](int index, const char *key, const char *name, bool available,
                    float remaining, float cooldown) {
        const float x = 14.0f + index * 144.0f;
        const float time = std::isfinite(remaining) ? (std::max)(0.0f, remaining) : 0;
        const bool ready = available && current > 0 && time <= 0;
        draw->AddRectFilled(point(x, 58), point(x + 136, 129), IM_COL32(29, 31, 28, 255), 3 * scale);
        draw->AddRect(point(x, 58), point(x + 136, 129), ready ? border : IM_COL32(65, 66, 59, 255), 3 * scale);
        label(x + 8, 64, 12, gold, key);
        centered(x, 82, 136, 17, available ? text : muted, name);
        char status[24];
        if (!available || current <= 0)
            std::snprintf(status, sizeof(status), "Unavailable");
        else if (ready)
            std::snprintf(status, sizeof(status), "Ready");
        else
            std::snprintf(status, sizeof(status), "%.1fs", std::ceil(time * 10) / 10);
        centered(x, 104, 136, 12, ready ? gold : muted, status);
        const float progress = !available || current <= 0 ? 0 : ready ? 1 :
            cooldown > 0 ? (std::clamp)(1 - time / cooldown, 0.0f, 1.0f) : 0;
        draw->AddRectFilled(point(x + 6, 122), point(x + 130, 125), IM_COL32(47, 48, 42, 255));
        if (progress > 0)
            draw->AddRectFilled(point(x + 6, 122), point(x + 6 + 124 * progress, 125), ready ? gold : border);
    };
    const auto *melee = registry.try_get<MeleeAttack>(player);
    const auto *axe = registry.try_get<AxeThrow>(player);
    const auto *aoe = registry.try_get<WarriorAOE>(player);
    slot(0, "LEFT CLICK", "Melee", melee != nullptr, melee ? melee->remaining : 0, melee ? melee->cooldown : 0);
    slot(1, "RIGHT CLICK", "Axe throw", axe != nullptr, axe ? axe->remaining : 0, axe ? axe->cooldown : 0);
    slot(2, "1", "AOE slam", aoe != nullptr, aoe ? aoe->remaining : 0, aoe ? aoe->cooldown : 0);
}
} // namespace

void RenderScreens(entt::registry &registry)
{
    switch (CurrentState.ActiveScreen) {
        case Screens::Playing:
            DrawPlayerHUD(registry);
            break;
        case Screens::MainMenu:
            DrawBlackWindow();
            DrawMainMenu(registry);
            break;
        case Screens::PauseMenu:
            DrawPauseMenu();
            break;
        case Screens::GameOverMenu:
            DrawBlackWindow();
            DrawGameOver(registry);
            break;
        case Screens::WinMenu:
            DrawBlackWindow();
            DrawWinScreen(registry);
            break;
    }
}

namespace {
enum class MenuAction { Start, Resume, MainMenu, Restart, Quit };

struct MenuPresentation {
    std::string subtitle, title, tagline, footer;
    std::map<MenuAction, std::string> buttons;
};

struct MenuSettings {
    // Storage only: every presentation value is required from game.ini.
    ImVec4 background{}, parchment{}, brass{}, gold{}, highlight{}, inkText{}, mutedText{},
        danger{};
    ImVec4 button{}, buttonHovered{}, buttonActive{};
    float panelWidth{}, buttonWidth{}, buttonHeight{}, buttonSpacing{};
    float mainTitleSize{}, headingSize{}, subtitleSize{}, footerSize{}, buttonTextSize{};
    MenuPresentation mainMenu, pause, gameOver, victory;
};

MenuSettings menuSettings;

std::string RequireMenuValue(const GameConfig &settings, const char *section, const char *key)
{
    const auto found = settings.find(section);
    if (found == settings.end() || found->second.find(key) == found->second.end())
        throw std::runtime_error(std::string("Missing required menu configuration: ") + section +
                                 "." + key);
    return found->second.at(key).as<std::string>();
}

MenuSettings LoadMenuSettings(const GameConfig &settings)
{
    MenuSettings result;
    auto text = [&](const char *section, const char *key, std::string &value) {
        value = RequireMenuValue(settings, section, key);
        if (value.empty() || value.find("##") != std::string::npos)
            throw std::runtime_error(std::string("Invalid menu text: ") + section + "." + key);
    };
    auto color = [&](const char *key, ImVec4 &value) {
        std::istringstream input(RequireMenuValue(settings, "MenuTheme", key));
        char comma1 = 0, comma2 = 0;
        ImVec4 parsed(0, 0, 0, 1);
        if (!(input >> parsed.x >> comma1 >> parsed.y >> comma2 >> parsed.z) || comma1 != ',' ||
            comma2 != ',' || !(input >> std::ws).eof() || !std::isfinite(parsed.x) ||
            !std::isfinite(parsed.y) || !std::isfinite(parsed.z) || parsed.x < 0 || parsed.x > 1 ||
            parsed.y < 0 || parsed.y > 1 || parsed.z < 0 || parsed.z > 1)
            throw std::runtime_error(std::string("Invalid MenuTheme.") + key +
                                     ": expected R,G,B in [0,1]");
        value = parsed;
    };
    color("background", result.background);
    color("parchment", result.parchment);
    color("brass", result.brass);
    color("gold", result.gold);
    color("highlight", result.highlight);
    color("text", result.inkText);
    color("mutedText", result.mutedText);
    color("danger", result.danger);
    color("button", result.button);
    color("buttonHovered", result.buttonHovered);
    color("buttonActive", result.buttonActive);
    auto dimension = [&](const char *key, float &value, float minimum, float maximum) {
        RequireMenuValue(settings, "MenuLayout", key);
        value = settings.RequireFloat("MenuLayout", key);
        if (value < minimum || value > maximum)
            throw std::runtime_error(std::string("Invalid MenuLayout.") + key +
                                     ": outside supported range");
    };
    dimension("panelWidth", result.panelWidth, 520, 800);
    dimension("buttonWidth", result.buttonWidth, 160, 440);
    dimension("buttonHeight", result.buttonHeight, 36, 60);
    dimension("buttonSpacing", result.buttonSpacing, 8, 20);
    dimension("mainTitleSize", result.mainTitleSize, 24, 80);
    dimension("headingSize", result.headingSize, 20, 48);
    dimension("subtitleSize", result.subtitleSize, 10, 18);
    dimension("footerSize", result.footerSize, 10, 18);
    dimension("buttonTextSize", result.buttonTextSize, 12, 26);
    auto screen = [&](const char *section, MenuPresentation &presentation,
                      std::initializer_list<MenuAction> actions) {
        text(section, "title", presentation.title);
        text(section, "subtitle", presentation.subtitle);
        text(section, "tagline", presentation.tagline);
        text(section, "footer", presentation.footer);
        for (auto action : actions) {
            const char *key = nullptr;
            switch (action) {
                case MenuAction::Start:
                    key = "startLabel";
                    break;
                case MenuAction::Resume:
                    key = "resumeLabel";
                    break;
                case MenuAction::MainMenu:
                    key = "mainMenuLabel";
                    break;
                case MenuAction::Restart:
                    key = "restartLabel";
                    break;
                case MenuAction::Quit:
                    key = "quitLabel";
                    break;
            }
            text(section, key, presentation.buttons[action]);
        }
    };
    screen("MainMenu", result.mainMenu, {MenuAction::Start, MenuAction::Quit});
    screen("PauseMenu", result.pause, {MenuAction::Resume, MenuAction::MainMenu, MenuAction::Quit});
    screen("GameOverMenu", result.gameOver, {MenuAction::MainMenu, MenuAction::Restart});
    screen("VictoryMenu", result.victory, {MenuAction::MainMenu, MenuAction::Restart});
    return result;
}
constexpr ImGuiCol menuColors[] = {
    ImGuiCol_WindowBg,        ImGuiCol_Border,    ImGuiCol_Text,
    ImGuiCol_TextDisabled,    ImGuiCol_Button,    ImGuiCol_ButtonHovered,
    ImGuiCol_ButtonActive,    ImGuiCol_FrameBg,   ImGuiCol_FrameBgHovered,
    ImGuiCol_FrameBgActive,   ImGuiCol_Header,    ImGuiCol_HeaderHovered,
    ImGuiCol_HeaderActive,    ImGuiCol_Separator, ImGuiCol_SeparatorHovered,
    ImGuiCol_SeparatorActive, ImGuiCol_NavCursor};

struct MenuLayout {
    ImVec2 origin;
    float center, top, scale;
    int buttonIndex = 0;
};

void MenuText(const MenuLayout &layout, const char *text, float y, float size, const ImVec4 &color)
{
    ImGui::PushFont(nullptr, size * layout.scale);
    float width = ImGui::CalcTextSize(text).x;
    const float availableWidth = (menuSettings.panelWidth - 64) * layout.scale;
    if (width > availableWidth) {
        ImGui::PopFont();
        ImGui::PushFont(nullptr, size * layout.scale * availableWidth / width);
        width = ImGui::CalcTextSize(text).x;
    }
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(layout.center - width * 0.5f, layout.top + y * layout.scale),
        ImGui::ColorConvertFloat4ToU32(color), text);
    ImGui::PopFont();
}

void MenuDiamond(ImDrawList *draw, ImVec2 center, float radius, ImU32 color)
{
    const ImVec2 points[] = {{center.x, center.y - radius},
                             {center.x + radius, center.y},
                             {center.x, center.y + radius},
                             {center.x - radius, center.y}};
    draw->AddPolyline(points, 4, color, ImDrawFlags_Closed, 1.0f);
}

ImU32 MenuColor(ImVec4 color, float alpha = 1.0f)
{
    color.w = alpha;
    return ImGui::ColorConvertFloat4ToU32(color);
}

void DrawMenuFrame(const MenuLayout &layout, bool mainMenu, bool gameOver)
{
    const auto *viewport = ImGui::GetMainViewport();
    const auto size = viewport->Size;
    auto *draw = ImGui::GetWindowDrawList();
    const float cx = layout.center, top = layout.top, s = layout.scale;
    const float halfWidth = menuSettings.panelWidth * 0.5f;
    const ImU32 edge = ImGui::ColorConvertFloat4ToU32(menuSettings.brass);
    const ImU32 accent =
        ImGui::ColorConvertFloat4ToU32(gameOver ? menuSettings.danger : menuSettings.gold);
    // Inset stage and manuscript borders; all decoration belongs to this one menu window.
    draw->AddRectFilled(viewport->Pos, ImVec2(viewport->Pos.x + size.x, viewport->Pos.y + size.y),
                        MenuColor(menuSettings.background, mainMenu ? 1.0f : 235 / 255.0f));
    draw->AddRectFilled(ImVec2(cx - halfWidth * s, top), ImVec2(cx + halfWidth * s, top + 610 * s),
                        ImGui::ColorConvertFloat4ToU32(menuSettings.parchment));
    draw->AddRect(ImVec2(cx - (halfWidth - 10) * s, top + 10 * s),
                  ImVec2(cx + (halfWidth - 10) * s, top + 600 * s), edge);
    draw->AddRect(ImVec2(cx - (halfWidth - 18) * s, top + 18 * s),
                  ImVec2(cx + (halfWidth - 18) * s, top + 592 * s),
                  MenuColor(menuSettings.brass, 65 / 255.0f));
    for (int side : {-1, 1}) {
        const float x = cx + side * (halfWidth - 31) * s;
        for (float y : {top + 31 * s, top + 579 * s}) {
            draw->AddLine(ImVec2(x - 9 * s, y), ImVec2(x + 9 * s, y), edge);
            draw->AddLine(ImVec2(x, y - 9 * s), ImVec2(x, y + 9 * s), edge);
            MenuDiamond(draw, ImVec2(x, y), 4 * s, accent);
        }
        // Sparse theater wings, behind the manuscript.
        for (int i = 0; i < 3; ++i) {
            const float wing = cx + side * (halfWidth + 40 + 22 * i) * s;
            draw->AddLine(ImVec2(wing, top + (90 + 20 * i) * s),
                          ImVec2(wing, top + (520 - 20 * i) * s),
                          MenuColor(menuSettings.brass, 25 / 255.0f));
        }
    }
}

void DrawMenuSeal(const MenuLayout &layout, bool gameOver)
{
    auto *draw = ImGui::GetWindowDrawList();
    const float cx = layout.center, top = layout.top, s = layout.scale;
    const auto edge = MenuColor(menuSettings.brass);
    const auto accent = MenuColor(gameOver ? menuSettings.danger : menuSettings.gold);
    // Original seal: a pierced diamond, concentric rings, and a split crown.
    const ImVec2 seal(cx, top + 107 * s);
    MenuDiamond(draw, seal, 31 * s, accent);
    draw->AddCircle(seal, 21 * s, edge, 32);
    draw->AddCircle(seal, 8 * s, accent, 24);
    draw->AddLine(ImVec2(cx, seal.y - 40 * s), ImVec2(cx, seal.y + 40 * s), edge);
    draw->AddLine(ImVec2(cx - 10 * s, seal.y - 24 * s), ImVec2(cx, seal.y - 40 * s), edge);
    draw->AddLine(ImVec2(cx + 10 * s, seal.y - 24 * s), ImVec2(cx, seal.y - 40 * s), edge);
}

MenuLayout BeginMenu(const char *id, const MenuPresentation &presentation, bool mainMenu = false,
                     bool gameOver = false)
{
    const auto *viewport = ImGui::GetMainViewport();
    const auto size = viewport->Size;
    const float scale =
        (std::max)(0.1f, (std::min)(1.4f, (std::min)(size.x / 1280.0f, size.y / 720.0f)));
    MenuLayout layout{viewport->Pos, viewport->Pos.x + size.x * 0.5f,
                      viewport->Pos.y + (size.y - 610 * scale) * 0.5f, scale};
    for (auto color : menuColors)
        ImGui::PushStyleColor(color, buttonStyle.Colors[color]);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 1.5f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scale, 10 * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12 * scale, 14 * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin(id, nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse);

    DrawMenuFrame(layout, mainMenu, gameOver);
    DrawMenuSeal(layout, gameOver);
    auto *draw = ImGui::GetWindowDrawList();
    const float cx = layout.center, top = layout.top, s = scale;
    const auto edge = MenuColor(menuSettings.brass);
    const auto accent = MenuColor(gameOver ? menuSettings.danger : menuSettings.gold);
    MenuText(layout, presentation.subtitle.c_str(), 47, menuSettings.subtitleSize,
             menuSettings.mutedText);
    MenuText(layout, presentation.title.c_str(), 162,
             mainMenu ? menuSettings.mainTitleSize : menuSettings.headingSize,
             gameOver ? menuSettings.inkText : menuSettings.highlight);
    MenuText(layout, presentation.tagline.c_str(), 242, menuSettings.subtitleSize,
             menuSettings.brass);
    const float dividerY = top + 285 * s;
    draw->AddLine(ImVec2(cx - 180 * s, dividerY), ImVec2(cx - 17 * s, dividerY), edge);
    draw->AddLine(ImVec2(cx + 17 * s, dividerY), ImVec2(cx + 180 * s, dividerY), edge);
    MenuDiamond(draw, ImVec2(cx, dividerY), 5 * s, accent);
    MenuText(layout, presentation.footer.c_str(), 556, menuSettings.footerSize,
             menuSettings.mutedText);
    ImGui::PushFont(nullptr, menuSettings.buttonTextSize * s);
    return layout;
}

bool MenuButton(MenuLayout &layout, const char *label)
{
    const float s = layout.scale;
    const float labelWidth = ImGui::CalcTextSize(label).x;
    const float availableWidth = (menuSettings.buttonWidth - 32) * s;
    const bool fitLabel = labelWidth > availableWidth;
    if (fitLabel)
        ImGui::PushFont(nullptr, menuSettings.buttonTextSize * s * availableWidth / labelWidth);
    ImGui::SetCursorScreenPos(
        ImVec2(layout.center - menuSettings.buttonWidth * 0.5f * s,
               layout.top + (324 + layout.buttonIndex++ *
                                       (menuSettings.buttonHeight + menuSettings.buttonSpacing)) *
                                s));
    const bool pressed =
        ImGui::Button(label, ImVec2(menuSettings.buttonWidth * s, menuSettings.buttonHeight * s));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    auto *draw = ImGui::GetWindowDrawList();
    const auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    draw->AddRect(
        a, b, ImGui::ColorConvertFloat4ToU32(hovered ? menuSettings.highlight : menuSettings.brass),
        1.5f * s);
    // Overlay only the label for hover/press contrast; hitboxes remain native ImGui buttons.
    if (hovered || active) {
        const auto textSize = ImGui::CalcTextSize(label);
        draw->AddRectFilled(
            ImVec2(a.x + 2, a.y + 2), ImVec2(b.x - 2, b.y - 2),
            ImGui::ColorConvertFloat4ToU32(
                buttonStyle.Colors[active ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered]));
        draw->AddText(
            ImVec2((a.x + b.x - textSize.x) * 0.5f, (a.y + b.y - textSize.y) * 0.5f),
            ImGui::ColorConvertFloat4ToU32(active ? menuSettings.inkText : menuSettings.highlight),
            label);
    }
    if (fitLabel)
        ImGui::PopFont();
    return pressed;
}

void EndMenu()
{
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar(8);
    ImGui::PopStyleColor(static_cast<int>(sizeof(menuColors) / sizeof(menuColors[0])));
}
// Owns exactly one menu frame, including its font and style stacks.
class ScopedMenu {
    const MenuPresentation &presentation;
    MenuLayout layout;

  public:
    ScopedMenu(const char *id, const MenuPresentation &data, bool mainMenu = false,
               bool gameOver = false)
        : presentation(data), layout(BeginMenu(id, data, mainMenu, gameOver))
    {
    }
    ~ScopedMenu()
    {
        EndMenu();
    }
    ScopedMenu(const ScopedMenu &) = delete;
    ScopedMenu &operator=(const ScopedMenu &) = delete;
    bool Button(MenuAction action)
    {
        const auto &label = presentation.buttons.at(action);
        ImGui::PushID(
            static_cast<int>(action)); // Labels never determine which action is performed.
        const bool pressed = MenuButton(layout, label.c_str());
        ImGui::PopID();
        return pressed;
    }
};
} // namespace

void ButtonStyleSetup()
{
    menuSettings = LoadMenuSettings(GameConfig{}); // Once at startup; never from the frame loop.
    buttonStyle = ImGui::GetStyle();
    auto *colors = buttonStyle.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_Border] = menuSettings.brass;
    colors[ImGuiCol_Text] = menuSettings.inkText;
    colors[ImGuiCol_TextDisabled] = menuSettings.mutedText;
    colors[ImGuiCol_Button] = menuSettings.button;
    colors[ImGuiCol_ButtonHovered] = menuSettings.buttonHovered;
    colors[ImGuiCol_ButtonActive] = menuSettings.buttonActive;
    colors[ImGuiCol_FrameBg] = colors[ImGuiCol_Button];
    colors[ImGuiCol_FrameBgHovered] = colors[ImGuiCol_ButtonHovered];
    colors[ImGuiCol_FrameBgActive] = colors[ImGuiCol_ButtonActive];
    colors[ImGuiCol_Header] = colors[ImGuiCol_Button];
    colors[ImGuiCol_HeaderHovered] = colors[ImGuiCol_ButtonHovered];
    colors[ImGuiCol_HeaderActive] = colors[ImGuiCol_ButtonActive];
    colors[ImGuiCol_Separator] = menuSettings.brass;
    colors[ImGuiCol_SeparatorHovered] = menuSettings.gold;
    colors[ImGuiCol_SeparatorActive] = menuSettings.highlight;
    colors[ImGuiCol_NavCursor] = menuSettings.highlight;
}

void DrawMainMenu(entt::registry &registry)
{
    ScopedMenu menu("##mainmenu", menuSettings.mainMenu, true);
    if (menu.Button(MenuAction::Start)) {
        RestartGame(registry);
    }
    if (menu.Button(MenuAction::Quit))
        CurrentState.Running = false;
}

void DrawPauseMenu()
{
    ScopedMenu menu("##pausemenu", menuSettings.pause);
    if (menu.Button(MenuAction::Resume))
        ChangeScreen(Screens::Playing);
    if (menu.Button(MenuAction::MainMenu))
        ChangeScreen(Screens::MainMenu);
    if (menu.Button(MenuAction::Quit))
        CurrentState.Running = false;
}

void DrawGameOver(entt::registry &reg)
{
    ScopedMenu menu("##gameover", menuSettings.gameOver, false, true);
    if (menu.Button(MenuAction::MainMenu))
        ChangeScreen(Screens::MainMenu);
    if (menu.Button(MenuAction::Restart))
        RestartGame(reg);
}

void DrawWinScreen(entt::registry &registry)
{
    ScopedMenu menu("##win", menuSettings.victory);
    if (menu.Button(MenuAction::MainMenu))
        ChangeScreen(Screens::MainMenu);
    if (menu.Button(MenuAction::Restart))
        RestartGame(registry);
}

void DrawBlackWindow()
{
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        viewport->Pos,
        ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
        MenuColor(menuSettings.background));
}

entt::entity CreateVampireQueen(entt::registry &registry, const CharacterConfig &config,
                                const VampiricDrainAttack &drain,
                                const GW::MATH::GMATRIXF &placement)
{
    const auto queen = CreateRenderable(registry, config.model, placement);
    try {
        // whichKeyToDrop is currently unused; DropDungeonKey selects its existing model.
        registry.emplace<Enemy>(queen, Enemy{true, 0});
        registry.emplace<VampireQueen>(queen);
        AttachCharacter(registry, queen, config);
        auto attack = drain;
        attack.remaining = 0;
        registry.emplace<VampiricDrainAttack>(queen, attack);
    } catch (...) {
        registry.destroy(queen);
        throw;
    }
    return queen;
}

entt::entity CreatUndeadKnight(entt::registry &registry,const CharacterConfig &config,
                                    const SpawnSquiresAttack &special,
                                    const RingAttackStats &special2,
                                    const GW::MATH::GMATRIXF &placement)
{
    const auto Knight = CreateRenderable(registry,config.model,placement);

  try
  {
    registry.emplace<Enemy>(Knight,Enemy{true,0});
    registry.emplace<UndeadKnight>(Knight);
    AttachCharacter(registry,Knight,config);
    auto SpawnSquires = special;
    auto RingAttack = special2;
    SpawnSquires.Remaining = 0;
    RingAttack.Remaining = 0;
    registry.emplace<SpawnSquiresAttack>(Knight,SpawnSquires);
    registry.emplace<RingAttackStats>(Knight,RingAttack);
  }

  catch(...) 
  {
    registry.destroy(Knight);
    throw;
  }
  return Knight;
}

float GroundDistance(entt::registry &registry, entt::entity a, entt::entity b)
{
    const auto &p = registry.get<Transform>(a).matrix.row4;
    const auto &q = registry.get<Transform>(b).matrix.row4;
    const float x = q.x - p.x, z = q.z - p.z;
    return std::sqrt(x * x + z * z);
}


void LoadLevel(entt::registry& registry, const std::string& levelKey)
{
    entt::entity display = entt::null;
    auto saveState = registry.ctx().get<DungeonState>();

    for(auto entity : registry.view<DRAW::VulkanRenderer>())
    {
        display = entity;
        break;
    }


    std::vector<entt::entity> entities;

    for(auto check : registry.view<entt::entity>())
    {
        entities.push_back(check);
    }

    for(auto check : entities)
    {
        if(check != display)
        registry.destroy(check);
    }


    registry.ctx().erase<DRAW::ModelManager>();
    registry.ctx().erase<DRAW::CPULevel>();
    registry.ctx().erase<DRAW::GPULevel>();
    registry.ctx().erase<CharacterConfig>();

    const auto& settings = *registry.ctx().get<UTIL::Config>().gameConfig;
    std::string LevelPath = settings.at("Content").at(levelKey).as<std::string>(); 

    registry.emplace<DRAW::CPULevel>(display,DRAW::CPULevel{LevelPath,settings.at("Content").at("models").as<std::string>(),{}});
    registry.ctx().emplace<DungeonState>(saveState);

    registry.emplace<DRAW::GPULevel>(display);

    GAME::InitializeGameplay(registry);

    ChangeScreen(Screens::Playing);

}

} // namespace GAME
