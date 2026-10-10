#pragma once
namespace GAME {
    
void InitializeGameplay(entt::registry& registry);
void UpdateGameSystems(entt::registry& registry);
#include <Gateware.h>
#include <entt/entt.hpp>
#include <string>
#include "imgui/imgui.h"


namespace GAME {
struct CharacterConfig;

// structs and what not for the screens
enum class Screens 
	{
		MainMenu,
		PauseMenu,
		GameOverMenu,
		Playing
	};

	struct GameState
    {
        Screens ActiveScreen = Screens::MainMenu;
        bool Running = true;
    };

struct ButtonDim
{
  
    float width = 200;
    float height =100;
};

    extern GameState CurrentState;
    extern GW::INPUT::GInput gInput;
    extern ImGuiStyle buttonStyle;
    extern ButtonDim buttonDiemension;
// functions for screens
void ChangeScreen(Screens s);
void DrawMainMenu();
void DrawPauseMenu();
void DrawGameOver();
void RenderScreens(entt::registry &registry);
void DrawBlackWindow();
void ButtonStyleSetup();



void InitializeGameplay(entt::registry &registry);
void RestartGame(entt::registry &registry);
void UpdateGameSystems(entt::registry &registry);
// Clone a loaded dynamic template by its exact Blender object name.
// Throws for an unavailable model. The returned entity owns its mesh instances.
entt::entity CreateRenderable(entt::registry& registry, const std::string& modelName,
                              const GW::MATH::GMATRIXF& transform);
}
