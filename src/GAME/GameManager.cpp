#include "GameManager.h"
#include "GameComponents.h"
#include "../DRAW/DrawComponents.h"
#include "../UTIL/Utilities.h"

namespace GAME
{
void UpdateGameSystems(entt::registry& registry)
{
    const float deltaTime = static_cast<float>(registry.ctx().get<UTIL::DeltaTime>().dtSec);
    for (auto entity : registry.view<Transform, Velocity>()) {
        auto& transform = registry.get<Transform>(entity).matrix;
        const auto& velocity = registry.get<Velocity>(entity).vector;
        transform.row4.x += velocity.x * deltaTime;
        transform.row4.y += velocity.y * deltaTime;
        transform.row4.z += velocity.z * deltaTime;
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
