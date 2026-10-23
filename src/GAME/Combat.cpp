#include "GameComponents.h"
#include "GameManager.h"
#include "GameplaySystems.h"
#include "../DRAW/DrawComponents.h"
#include <random>
#include <iostream>

namespace GAME {

    // Tracks the number of enemies defeated during the current game.
    static int totalEnemyKills = 0;

    // Resets the kill counter when a new game begins.
    void ResetEnemyKillCounter()
    {
        totalEnemyKills = 0;

        std::cout << "New Game Started! Total Kills: 0" << std::endl;
    }

void UpdateCombatTimers(entt::registry &registry, float deltaTime)
{
    for (auto entity : registry.view<AxeThrow>()) {
        auto &timer = registry.get<AxeThrow>(entity).remaining;
        timer = (std::max)(0.0f, timer - deltaTime);
    }
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
    // Start with the enemy's normal attack damage
    float finalDamage = attack.damage;

    // If the player has an active defense potion, reduce incoming damage
    if (registry.all_of<Player, DefenseBoost>(target))
    {
        const auto& defense = registry.get<DefenseBoost>(target);

        if (defense.remaining > 0.0f)
        {
            finalDamage *= (1.0f - defense.damageReduction);
        }
    }

    // Apply the final damage after checking for defense
    health.current = (std::max)(0.0f, health.current - finalDamage);
    if (health.current < previousHealth)
        ActivateDamageFlash(registry, target);
    const char *source = registry.all_of<Player>(attacker) ? "Player" : "Enemy";
    const char *victim = registry.all_of<Player>(target) ? "Player" : "Enemy";
    // Print the actual damage dealt after checking defense.
    std::cout << source << " attacked " << victim << ": "
        << finalDamage << " damage\n"
        << victim << " health: " << health.current
        << " / " << health.maximum << std::endl;
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

void TryThrowAxe(entt::registry &registry, entt::entity player,
                 const GW::MATH::GVECTORF &target)
{
    if (!registry.valid(player) ||
        !registry.all_of<Player, Transform, Health, MeleeAttack, AxeThrow>(player) ||
        registry.any_of<ToDestroy>(player) || registry.get<Health>(player).current <= 0)
        return;
    auto &attack = registry.get<AxeThrow>(player);
    if (attack.remaining > 0)
        return;
    const auto origin = registry.get<Transform>(player).matrix.row4;
    const float dx = target.x - origin.x, dz = target.z - origin.z;
    const float distance = std::sqrt(dx * dx + dz * dz);
    if (!std::isfinite(distance) || distance < 0.0001f)
        return;
    auto transform = GW::MATH::GIdentityMatrixF;
    transform.row4 = {origin.x, origin.y + 0.6f, origin.z, 1};
    const auto axe = CreateRenderable(registry, "ThrowingAxe", transform);
    try {
        registry.emplace<ThrownAxe>(axe, ThrownAxe{
            {dx / distance * attack.speed, 0, dz / distance * attack.speed, 0},
            registry.get<MeleeAttack>(player).damage * attack.damageMultiplier,
            attack.range / attack.speed});
    } catch (...) {
        registry.destroy(axe);
        throw;
    }
    // Independent of melee and AOE cooldowns; invalid aiming consumes no throw.
    attack.remaining = attack.cooldown;
}

void UpdateThrownAxes(entt::registry &registry, float deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime < 0)
        return;
    for (auto axe : registry.view<ThrownAxe, Transform>(entt::exclude<ToDestroy>)) {
        auto &shot = registry.get<ThrownAxe>(axe);
        auto &transform = registry.get<Transform>(axe).matrix;
        const auto origin = transform.row4;
        const float dt = (std::min)(deltaTime, (std::max)(0.0f, shot.remaining));
        const float dx = shot.velocity.x * dt, dz = shot.velocity.z * dt;
        const float lengthSquared = dx * dx + dz * dz;
        float earliest = 2.0f;
        entt::entity victim = entt::null;
        // Segment/circle entry time catches fast throws and chooses the first enemy,
        // regardless of EnTT's iteration order. Only enemies can be hit.
        for (auto enemy : registry.view<Enemy, Transform, Health>(entt::exclude<ToDestroy>)) {
            if (registry.get<Health>(enemy).current <= 0)
                continue;
            const auto position = registry.get<Transform>(enemy).matrix.row4;
            const float x = origin.x - position.x, z = origin.z - position.z;
            const float c = x * x + z * z - shot.hitRadius * shot.hitRadius;
            float t = 0;
            if (c > 0) {
                if (lengthSquared <= 0)
                    continue;
                const float b = x * dx + z * dz;
                const float discriminant = b * b - lengthSquared * c;
                if (discriminant < 0)
                    continue;
                t = (-b - std::sqrt(discriminant)) / lengthSquared;
                if (t < 0 || t > 1)
                    continue;
            }
            if (t < earliest || (t == earliest && entt::to_integral(enemy) < entt::to_integral(victim))) {
                earliest = t;
                victim = enemy;
            }
        }
        const float travel = victim == entt::null ? 1.0f : earliest;
        shot.angle += dt * 18.0f;
        const float cosine = std::cos(shot.angle), sine = std::sin(shot.angle);
        transform.row1 = {cosine, 0, -sine, 0};
        transform.row3 = {sine, 0, cosine, 0};
        transform.row4 = {origin.x + dx * travel, origin.y, origin.z + dz * travel, 1};
        shot.remaining -= dt;
        if (victim != entt::null) {
            auto &health = registry.get<Health>(victim);
            health.current = (std::max)(0.0f, health.current - shot.damage);
            ActivateDamageFlash(registry, victim);
            registry.emplace<ToDestroy>(axe);
        } else if (shot.remaining <= 0) {
            registry.emplace<ToDestroy>(axe);
        }
    }
    HandleEntityDeath(registry);
}

// Gives enemies a chance to drop a random potion when defeated.
void DropRandomPotion(entt::registry& registry, const Transform& enemyTransform)
{
    // Set up our random number generator.
    static std::random_device randomDevice;
    static std::mt19937 generator(randomDevice());

    // Roll a number between 1 and 100.
    std::uniform_int_distribution<int> dropChance(1, 100);

    // Only 30% of defeated enemies will drop a potion.
    if (dropChance(generator) > 30)
        return;

    // Pick one of our three potion types.
    std::uniform_int_distribution<int> potionType(1, 3);
    int selectedPotion = potionType(generator);

    // Spawn the potion where the enemy died.
    auto potionTransform = enemyTransform;

    // Choose which potion model and effect to use.
    const char* modelName = nullptr;
    BounsStatPickUpType pickupType = BounsStatPickUpType::HealthRestore;
    float pickupValue = 0.0f;

    switch (selectedPotion)
    {
    case 1:
        // Red potion restores 25 HP.
        modelName = "HealthPotion";
        pickupType = BounsStatPickUpType::HealthRestore;
        pickupValue = 25.0f;
        break;

    case 2:
        // Green potion gives a temporary speed boost.
        modelName = "SpeedPotion";
        pickupType = BounsStatPickUpType::BonusSpeed;
        pickupValue = 0.5f;
        break;

    case 3:
        // Blue potion gives temporary damage reduction.
        modelName = "DefensivePotion";
        pickupType = BounsStatPickUpType::DefenseBoost;
        pickupValue = 0.10f;
        break;
    }

    // Create the potion model in the world.
    const auto potion = CreateRenderable(
        registry,
        modelName,
        potionTransform.matrix
    );

    // Give the potion its pickup effect.
    registry.emplace<PickUp>(
        potion,
        PickUp{ pickupType, pickupValue }
    );

    // Print a message so we can confirm drops are working.
    std::cout << "Enemy dropped a " << modelName << "!" << std::endl;
}

void HandleEntityDeath(entt::registry& registry)
{
    // Check every entity that has a transform and health.
    for (auto victim : registry.view<Transform, Health>(entt::exclude<ToDestroy>))
    {
        auto& health = registry.get<Health>(victim);

        // Only process entities that have died.
        if (health.current <= 0)
        {
            // Check whether the defeated entity is an enemy.
            if (registry.all_of<Enemy>(victim))
            {
                // Count this enemy as defeated.
                totalEnemyKills++;

                // Print our updated kill count.
                std::cout << "Enemy defeated! Total Kills: "
                    << totalEnemyKills << std::endl;

                // Check if the defeated enemy was a boss.
                if (registry.get<Enemy>(victim).isBoss)
                {
                    // Bosses still drop their dungeon keys.
                    DropDungeonKey(registry, registry.get<Transform>(victim));

                    std::cout << "Boss defeated!" << std::endl;
                }

                // Every defeated enemy has a chance to drop a potion.
                DropRandomPotion(registry, registry.get<Transform>(victim));
            }

            // Keep the existing player death behavior.
            if (registry.all_of<Player>(victim))
            {
                ChangeScreen(Screens::GameOverMenu);
            }

            // Mark the defeated entity for cleanup.
            registry.emplace<ToDestroy>(victim);
        }
    }
}
} // namespace GAME
