#include "GameComponents.h"
#include "GameManager.h"
#include "GameplaySystems.h"

namespace GAME {

void TryVampiricDrain(entt::registry &registry, entt::entity target)
{
    if (!registry.valid(target) || !registry.all_of<Player, Transform, Health>(target) ||
        registry.any_of<ToDestroy>(target) || registry.get<Health>(target).current <= 0)
        return;
    for (auto queen : registry.view<VampireQueen, Enemy, Transform, Health, Movement, MeleeAttack,
                                    VampiricDrainAttack>(entt::exclude<ToDestroy>)) {
        auto &health = registry.get<Health>(queen);
        auto &attack = registry.get<VampiricDrainAttack>(queen);
        if (health.current <= 0 || attack.remaining > 0 ||
            GroundDistance(registry, queen, target) > attack.range)
            continue;
        attack.remaining = attack.cooldown;
        // Get the player's current health.
        auto& targetHealth = registry.get<Health>(target);
        const float previousHealth = targetHealth.current;

        // Start with the Vampire Queen's normal damage.
        float finalDamage = attack.damage;

        // Check whether the player has an active defense potion.
        if (registry.all_of<DefenseBoost>(target))
        {
            const auto& defense = registry.get<DefenseBoost>(target);

            // Reduce the Queen's damage while the potion is active.
            if (defense.remaining > 0.0f)
            {
                finalDamage *= (1.0f - defense.damageReduction);
            }
        }

        // Apply the reduced damage to the player.
        targetHealth.current = (std::max)(0.0f, targetHealth.current - finalDamage);

        // Calculate how much health the player actually lost.
        // The Queen's healing will use this reduced damage too.
        const float damage = previousHealth - targetHealth.current;
        const float previousQueenHealth = health.current;
        health.current = static_cast<float>(
            (std::min)(double(health.maximum),
                       double(health.current) + double(damage) * attack.lifeSteal));
        if (damage > 0)
            ActivateDamageFlash(registry, target);
        std::cout << "Vampire Queen drained Player: " << damage
                  << " damage; restored: " << health.current - previousQueenHealth
                  << " health\nPlayer health: " << targetHealth.current << " / "
                  << targetHealth.maximum << "\nVampire Queen health: " << health.current << " / "
                  << health.maximum << std::endl;
        HandleEntityDeath(registry);
        return; // This encounter has one active Queen.
    }
}

void UpdateVampireQueen(entt::registry &registry, float deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime < 0)
        return;
    entt::entity player = entt::null;
    for (auto candidate : registry.view<Player, Transform, Health>(entt::exclude<ToDestroy>)) {
        if (registry.get<Health>(candidate).current > 0) {
            player = candidate;
            break;
        }
    }
    if (player == entt::null)
        return;
    for (auto queen : registry.view<VampireQueen, Enemy, Transform, Movement, MeleeAttack,
                                    VampiricDrainAttack, Health>(entt::exclude<ToDestroy>)) {
        if (registry.get<Health>(queen).current <= 0 || registry.any_of<ToDestroy>(player) ||
            registry.get<Health>(player).current <= 0)
            continue;
        const float distance = GroundDistance(registry, queen, player);
        const auto &drain = registry.get<VampiricDrainAttack>(queen);
        const float range = registry.get<MeleeAttack>(queen).range;
        if (drain.remaining <= 0 && distance <= drain.range) {
            TryVampiricDrain(registry, player);
        } else if (distance <= range) {
            TryMeleeAttack(registry, queen, player);
        } else {
            const auto &target = registry.get<Transform>(player).matrix.row4;
            auto &position = registry.get<Transform>(queen).matrix.row4;
            const float step =
                (std::min)(registry.get<Movement>(queen).speed * deltaTime, distance - range);
            position.x += (target.x - position.x) / distance * step;
            position.z += (target.z - position.z) / distance * step;
        }
    }
}
} // namespace GAME
