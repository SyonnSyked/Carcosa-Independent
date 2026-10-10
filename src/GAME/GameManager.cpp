#include "GameManager.h"
#include "GameComponents.h"
#include "GameplaySystems.h"
#include "../DRAW/DrawComponents.h"
#include "../UTIL/Utilities.h"

namespace GAME
{
entt::entity CreateRenderable(entt::registry& registry, const std::string& modelName,
                              const GW::MATH::GMATRIXF& transform)
{
    const auto* manager = registry.ctx().find<DRAW::ModelManager>();
    if (!manager || !manager->models.contains(modelName))
        throw std::runtime_error("Renderable template not loaded: " + modelName +
                                 " (export it in the selected world with gameType != none)");
    const auto& source = manager->models.at(modelName);
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

void InitializeGameplay(entt::registry& registry)
{
    const auto& settings = *registry.ctx().get<UTIL::Config>().gameConfig;
    const auto levels = registry.view<DRAW::CPULevel>();
    const auto& level = registry.get<DRAW::CPULevel>(levels.front()).lvlData;
    for (const auto* role : {"Player", "Enemy"}) {
        const auto& tuning = settings.at(role);
        const auto name = tuning.at("model").as<std::string>();
        bool found = false;
        for (const auto& object : level.blenderObjects) {
            if (name != object.blendername) continue;
            const auto entity = CreateRenderable(registry, name, level.levelTransforms[object.transformIndex]);
            if (std::string(role) == "Player") registry.emplace<Player>(entity);
            else registry.emplace<Enemy>(entity);
            const float health = tuning.at("health").as<float>();
            registry.emplace<Health>(entity, Health{health, health});
            registry.emplace<Movement>(entity, Movement{tuning.at("speed").as<float>()});
            registry.emplace<MeleeAttack>(entity, MeleeAttack{tuning.at("damage").as<float>(),
                tuning.at("range").as<float>(), tuning.at("cooldown").as<float>()});
            found = true;
            break;
        }
        if (!found) throw std::runtime_error("Gameplay object not loaded: " + name);
    }
    UpdateCamera(registry);
    std::cout << "Gameplay ready: WASD moves, SPACE attacks (press again for each swing)." << std::endl;
}

void UpdateGameSystems(entt::registry& registry)
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
    registry.clear();
    InitializeGameplay(registry);
    ChangeScreen(Screens::Playing);
}




void UpdateGameSystems(entt::registry &registry)

{
    static bool prevEscaped = false;
    float state = 0.0f;
    gInput.GetState(G_KEY_ESCAPE, state);
    bool pressed = (state > 0.0f) && !prevEscaped;
    prevEscaped = (state > 0.0f);

    if (pressed)
    {
        if(CurrentState.ActiveScreen == Screens::Playing)
            ChangeScreen(Screens::PauseMenu);
        else if (CurrentState.ActiveScreen == Screens::PauseMenu)
            ChangeScreen(Screens::Playing);

    }


    switch(CurrentState.ActiveScreen)
    {
        case(Screens::Playing)://normal run the gameplay shit
        {
    const float deltaTime = static_cast<float>(registry.ctx().get<UTIL::DeltaTime>().dtSec);
    UpdateCombatTimers(registry, deltaTime);
    UpdatePlayer(registry, deltaTime);
    UpdateEnemy(registry, deltaTime);
    UpdateCamera(registry);

    for (auto entity : registry.view<Transform, Velocity>()) {
        auto& transform = registry.get<Transform>(entity).matrix;
        const auto& velocity = registry.get<Velocity>(entity).vector;
        transform.row4.x += velocity.x * deltaTime;
        transform.row4.y += velocity.y * deltaTime;
        transform.row4.z += velocity.z * deltaTime;
    IntegrateVelocity(registry, deltaTime);
    SynchronizeMeshTransforms(registry);
    DestroyPendingEntities(registry);
        break;
    }
        case(Screens::PauseMenu):
        case(Screens::MainMenu):
        case(Screens::GameOverMenu):
            break;

    }



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

    for (auto entity : registry.view<Transform, DRAW::MeshCollection>()) {
        const auto& transform = registry.get<Transform>(entity).matrix;
        const auto& meshes = registry.get<DRAW::MeshCollection>(entity).meshEntities;
        for (auto mesh : meshes)
            if (registry.valid(mesh) && registry.all_of<DRAW::GPUInstance>(mesh))
                registry.get<DRAW::GPUInstance>(mesh).transform = transform;
    }

    const auto pending = registry.view<ToDestroy>();
    std::vector<entt::entity> entities(pending.begin(), pending.end());
    for (auto entity : entities)
        if (registry.valid(entity)) registry.destroy(entity);
}
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


}// namespace GAME

