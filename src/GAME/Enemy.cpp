#include "GameComponents.h"
#include "GameManager.h"
#include "GameplaySystems.h"

namespace GAME {
void UpdateEnemy(entt::registry &registry, float deltaTime)
{
    for (auto player : registry.view<Player, Transform, Health>()) {
        for (auto enemy : registry.view<Enemy, Transform, Movement, MeleeAttack, Health>()) {
            const float distance = GroundDistance(registry, enemy, player);
            const float range = registry.get<MeleeAttack>(enemy).range;
            if (distance > range) {
                const auto &target = registry.get<Transform>(player).matrix.row4;
                auto &position = registry.get<Transform>(enemy).matrix.row4;
                // Stop at melee range, even on a long frame. Preserve exported ground height.
                const float step =
                    (std::min)(registry.get<Movement>(enemy).speed * deltaTime, distance - range);
                position.x += (target.x - position.x) / distance * step;
                position.z += (target.z - position.z) / distance * step;
            } else {
                TryMeleeAttack(registry, enemy, player);
            }
        }
    }
}
} // namespace GAME
