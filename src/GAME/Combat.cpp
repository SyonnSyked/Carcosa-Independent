#include "GameComponents.h"
#include "GameManager.h"
#include "GameplaySystems.h"

namespace GAME {

void UpdateCombatTimers(entt::registry &registry, float deltaTime)
{
    for (auto entity : registry.view<VampiricDrainAttack>()) {
        auto &timer = registry.get<VampiricDrainAttack>(entity).remaining;
        timer = (std::max)(0.0f, timer - deltaTime);
    }
    for (auto entity : registry.view<WarriorAOE>()) {
        auto &timer = registry.get<WarriorAOE>(entity).remaining;
        timer = (std::max)(0.0f, timer - deltaTime);
    }
    for (auto entity : registry.view<MeleeAttack>()) {
        auto &timer = registry.get<MeleeAttack>(entity).remaining;
        timer = (std::max)(0.0f, timer - deltaTime);
    }
}
void TryMeleeAttack(entt::registry &registry, entt::entity attacker, entt::entity target)
{
    auto &attack = registry.get<MeleeAttack>(attacker);
    auto &health = registry.get<Health>(target);
    if (registry.get<Health>(attacker).current <= 0 || health.current <= 0 || attack.remaining > 0)
        return;
    // A swing consumes its cooldown even when it misses.
    attack.remaining = attack.cooldown;
    if (GroundDistance(registry, attacker, target) > attack.range)
        return;
    const float previousHealth = health.current;
    health.current = (std::max)(0.0f, health.current - attack.damage);
    if (health.current < previousHealth)
        ActivateDamageFlash(registry, target);
    const char *source = registry.all_of<Player>(attacker) ? "Player" : "Enemy";
    const char *victim = registry.all_of<Player>(target) ? "Player" : "Enemy";
    std::cout << source << " attacked " << victim << ": " << attack.damage << " damage\n"
              << victim << " health: " << health.current << " / " << health.maximum << std::endl;
    HandleEntityDeath(registry);
}

void TryWarriorAOE(entt::registry &registry, entt::entity player)
{
    if (!registry.valid(player) ||
        !registry.all_of<Player, Transform, Health, WarriorAOE>(player) ||
        registry.any_of<ToDestroy>(player) || registry.get<Health>(player).current <= 0)
        return;
    auto &ability = registry.get<WarriorAOE>(player);
    if (ability.remaining > 0)
        return;
    ability.remaining = ability.cooldown;
    ActivateDamageFlash(registry, player);
    unsigned hits = 0;
    for (auto enemy : registry.view<Enemy, Transform, Health>(entt::exclude<ToDestroy>)) {
        auto &health = registry.get<Health>(enemy);
        if (health.current <= 0 || GroundDistance(registry, player, enemy) > ability.radius)
            continue;
        health.current = (std::max)(0.0f, health.current - ability.damage);
        ActivateDamageFlash(registry, enemy);
        ++hits;
        HandleEntityDeath(registry);
    }
    std::cout << "Warrior AOE hit " << hits << " enemies for " << ability.damage
              << " damage each. Cooldown: " << ability.cooldown << " seconds.\n";
}

void HandleEntityDeath(entt::registry &registry)
{
    for (auto victim : registry.view<Transform, Health>(entt::exclude<ToDestroy>)) {
        auto &health = registry.get<Health>(victim);
        if (health.current <= 0) {
            if (registry.all_of<Enemy>(victim)) {
                if (registry.get<Enemy>(victim).isBoss == true) {
                    DropDungeonKey(registry, registry.get<Transform>(victim));
                }
            }
            if (registry.all_of<Player>(victim))
                ChangeScreen(Screens::GameOverMenu);
            registry.emplace<ToDestroy>(victim);
        }
    }
}
} // namespace GAME
