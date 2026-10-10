#pragma once
#include <Gateware.h>
#include <entt/entt.hpp>
#include <string>
#include "GameComponents.h"

struct ImGuiStyle;

namespace GAME {
struct GameState {
    Screens ActiveScreen = Screens::MainMenu;
    bool Running = true;
};

struct ButtonDim {
    float width = 200;
    float height = 100;
};

extern GameState CurrentState;
extern ImGuiStyle buttonStyle;
extern ButtonDim buttonDiemension;

void ChangeScreen(Screens s);
void DrawMainMenu();
void DrawPauseMenu();
void DrawGameOver(entt::registry &registry);
void RenderScreens(entt::registry &registry);
void DrawBlackWindow();
void ButtonStyleSetup();

void InitializeGameplay(entt::registry &registry);
void RestartGame(entt::registry &registry);
void UpdateGameSystems(entt::registry &registry);
// Clone a loaded dynamic template by its exact Blender object name.
// Throws for an unavailable model. The returned entity owns its mesh instances.
entt::entity CreateRenderable(entt::registry &registry, const std::string &modelName,
                              const GW::MATH::GMATRIXF &transform);
entt::entity CreateEnemy(entt::registry &registry, const CharacterConfig &config,
                         const GW::MATH::GMATRIXF &placement);
} // namespace GAME
