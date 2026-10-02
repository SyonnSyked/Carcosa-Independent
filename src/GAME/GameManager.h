#pragma once
namespace GAME {
struct CharacterConfig;
void InitializeGameplay(entt::registry &registry);
void UpdateGameSystems(entt::registry &registry);
// Clone a loaded dynamic template by its exact Blender object name.
// Throws for an unavailable model. The returned entity owns its mesh instances.
entt::entity CreateRenderable(entt::registry &registry, const std::string &modelName,
                              const GW::MATH::GMATRIXF &transform);

entt::entity CreateEnemy(entt::registry &registry, const CharacterConfig &config,
                         const GW::MATH::GMATRIXF &placement);
} // namespace GAME
