#include "GameComponents.h"
#include "GameplaySystems.h"

namespace GAME {
float GroundDistance(entt::registry& registry, entt::entity a, entt::entity b)
{
    const auto& p = registry.get<Transform>(a).matrix.row4;
    const auto& q = registry.get<Transform>(b).matrix.row4;
    const float x = q.x - p.x, z = q.z - p.z;
    return std::sqrt(x*x + z*z);
}

void UpdateCombatTimers(entt::registry& registry, float deltaTime)
{
    for (auto entity : registry.view<MeleeAttack>()) {
        auto& timer = registry.get<MeleeAttack>(entity).remaining;
        timer = (std::max)(0.0f, timer - deltaTime);
    }
}

void TryMeleeAttack(entt::registry& registry, entt::entity attacker, entt::entity target)
{
    auto& attack = registry.get<MeleeAttack>(attacker);
    auto& health = registry.get<Health>(target);
    if (registry.get<Health>(attacker).current <= 0 || health.current <= 0 || attack.remaining > 0)
        return;
    // A swing consumes its cooldown even when it misses.
    attack.remaining = attack.cooldown;
    if (GroundDistance(registry, attacker, target) > attack.range)
        return;
    health.current = (std::max)(0.0f, health.current - attack.damage);
    const char* source = registry.all_of<Player>(attacker) ? "Player" : "Enemy";
    const char* victim = registry.all_of<Player>(target) ? "Player" : "Enemy";
    std::cout << source << " attacked " << victim << ": " << attack.damage << " damage\n"
              << victim << " health: " << health.current << " / " << health.maximum << std::endl;
    if (health.current == 0) {
        std::cout << victim << " died." << std::endl;
        // Keep the dead player visible and the camera anchored; enemies own removable meshes.
        if (registry.all_of<Enemy>(target)) registry.emplace<ToDestroy>(target);
    }
}
}
