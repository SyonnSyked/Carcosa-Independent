#pragma once
namespace GAME {
void UpdateGameSystems(entt::registry& registry);
// Clone a loaded dynamic template by its exact Blender object name.
// Throws for an unavailable model. The returned entity owns its mesh instances.
entt::entity CreateRenderable(entt::registry& registry, const std::string& modelName,
                              const GW::MATH::GMATRIXF& transform);
}
