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
    unsigned int playerScore = 0;
};

struct ButtonDim {
    float width = 200;
    float height = 100;
};

extern GameState CurrentState;
extern ImGuiStyle buttonStyle;
extern ButtonDim buttonDiemension;

void ChangeScreen(Screens s);
void DrawMainMenu(entt::registry &registry);
void DrawPauseMenu();
void DrawGameOver(entt::registry &registry);
void DrawWinScreen(entt::registry &registry);
void RenderScreens(entt::registry &registry);
void DrawBlackWindow();
void ButtonStyleSetup();

void DrawHighScore(entt::registry &registry);
void AddToPlayerScore(unsigned int points);

struct CharacterConfig;
struct VampiricDrainAttack;
entt::entity CreateVampireQueen(entt::registry &registry, const CharacterConfig &config,
                                const VampiricDrainAttack &drain,
                                const GW::MATH::GMATRIXF &placement);
void InitializeGameplay(entt::registry &registry);
void RestartGame(entt::registry &registry);
void UpdateGameSystems(entt::registry &registry);
// Clone a loaded dynamic template by its exact Blender object name.
// Throws for an unavailable model. The returned entity owns its mesh instances.
entt::entity CreateRenderable(entt::registry &registry, const std::string &modelName,
                              const GW::MATH::GMATRIXF &transform);
entt::entity CreateEnemy(entt::registry &registry, const CharacterConfig &config,
                         const GW::MATH::GMATRIXF &placement);
entt::entity CreateEnemyVariant(entt::registry &registry, const CharacterConfig &config,
                                const GW::MATH::GMATRIXF &placement, EnemyVariant variant);

float GroundDistance(entt::registry &registry, entt::entity a, entt::entity b);
} // namespace GAME
