#include "GameComponents.h"
#include "GameManager.h"
#include "GameplaySystems.h"

namespace GAME {
void DropDungeonKey(entt::registry &registry, const Transform placement)
{
    const entt::entity &dungeonKey = CreateRenderable(registry, "DungeonKey", placement.matrix);
}
void UseDungeonKey(entt::registry &registry)
{
    const auto player = registry.view<Player, Transform>().front();
    const auto door = registry.view<DungeonExit, Transform>().front();
    const auto key = registry.view<Player, Inventory, DungeonKey>().front();

    float distToDoor = GroundDistance(registry, door, player);

    if (distToDoor <= registry.get<DungeonKey>(key).distToUse) {
        // playerdungeon exit here, first win condition for when we have only 1 level
    }
}
} // namespace GAME
