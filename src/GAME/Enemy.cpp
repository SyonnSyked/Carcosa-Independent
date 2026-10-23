#include "../DRAW/DrawComponents.h"
#include "GameManager.h"
#include "GameComponents.h"
#include "GameplaySystems.h"
#include "../UTIL/Utilities.h"
#include <algorithm>

namespace GAME {
namespace {
void CastSpellArea(entt::registry &registry, entt::entity player, const WizardAttack &attack)
{
    const auto center = registry.get<Transform>(player).matrix.row4;
    const float heightOffset =
        registry.ctx().get<UTIL::Config>().gameConfig->RequireFloat("WizardWarning", "heightOffset");
    const auto &source = registry.ctx().get<DRAW::ModelManager>().models.at("PurpleSpellEffect");
    auto transform = registry.get<DRAW::GPUInstance>(source.meshEntities.at(0)).transform;
    // The authored XY ring has outer radius 1.25; its exported transform aligns it to XZ.
    // Scale only the ring plane, preserving thickness and the existing floor clearance.
    const float scale = attack.radius / 1.25f;
    transform.row1.x *= scale;
    transform.row1.y *= scale;
    transform.row1.z *= scale;
    transform.row2.x *= scale;
    transform.row2.y *= scale;
    transform.row2.z *= scale;
    transform.row4 = {center.x, center.y + heightOffset, center.z, 1};
    const auto area = CreateRenderable(registry, "PurpleSpellEffect", transform);
    try {
        // One model owns all its submeshes; synchronization preserves this single transform.
        registry.emplace<SpellArea>(area, SpellArea{center, attack.radius, attack.damage});
    } catch (...) {
        registry.destroy(area);
        throw;
    }
}
void FireProjectile(entt::registry &registry, entt::entity enemy, entt::entity player,
                    const RangedAttack &attack)
{
    const auto origin = registry.get<Transform>(enemy).matrix.row4;
    const auto target = registry.get<Transform>(player).matrix.row4;
    const float distance = GroundDistance(registry, enemy, player);
    constexpr float speed = 7.0f;
    GW::MATH::GMATRIXF transform;
    GW::MATH::GMatrix::IdentityF(transform);
    transform.row1.x = transform.row2.y = transform.row3.z = 0.15f;
    transform.row4 = {origin.x, origin.y, origin.z, 1};
    const auto projectile = CreateRenderable(registry, registry.ctx().get<CharacterConfig>().model,
                                              transform);
    try {
        registry.emplace<Projectile>(projectile, Projectile{
            {(target.x - origin.x) / distance * speed, 0,
             (target.z - origin.z) / distance * speed, 0}, attack.damage});
        for (auto mesh : registry.get<DRAW::MeshCollection>(projectile).meshEntities) {
            auto &material = registry.get<DRAW::GPUInstance>(mesh).matData;
            material.Kd = {1.0f, 0.6f, 0.05f};
            material.Ke = {1.0f, 0.4f, 0.0f};
        }
    } catch (...) {
        registry.destroy(projectile);
        throw;
    }
}
} // namespace
void UpdateEnemy(entt::registry &registry, float deltaTime)
{
    for (auto player : registry.view<Player, Transform, Health>(entt::exclude<ToDestroy>)) {
        if (registry.get<Health>(player).current <= 0)
            return;
        for (auto enemy : registry.view<Enemy, Transform, Movement, MeleeAttack, Health>(entt::exclude<ToDestroy, VampireQueen>)) {
            if (registry.get<Health>(enemy).current <= 0)
                continue;
            const float distance = GroundDistance(registry, enemy, player);
            auto *wizard = registry.try_get<WizardAttack>(enemy);
            if (wizard)
                wizard->remaining = (std::max)(0.0f, wizard->remaining - deltaTime);
            auto *ranged = registry.try_get<RangedAttack>(enemy);
            if (ranged)
                ranged->remaining = (std::max)(0.0f, ranged->remaining - deltaTime);
            const float range = wizard ? wizard->range :
                (ranged ? ranged->range : registry.get<MeleeAttack>(enemy).range);
            if (distance > range) {
                const auto &target = registry.get<Transform>(player).matrix.row4;
                auto &position = registry.get<Transform>(enemy).matrix.row4;
                // Stop at the attack range, even on a long frame. Preserve exported ground height.
                const float step =
                    (std::min)(registry.get<Movement>(enemy).speed * deltaTime, distance - range);
                position.x += (target.x - position.x) / distance * step;
                position.z += (target.z - position.z) / distance * step;
            } else {
                if (wizard) {
                    if (wizard->remaining <= 0) {
                        CastSpellArea(registry, player, *wizard);
                        wizard->remaining = wizard->cooldown;
                    }
                } else if (ranged) {
                    if (ranged->remaining <= 0 && distance > 0.0001f) {
                        FireProjectile(registry, enemy, player, *ranged);
                        ranged->remaining = ranged->cooldown;
                    }
                } else
                    TryMeleeAttack(registry, enemy, player);
                if (registry.get<Health>(player).current <= 0)
                    return;
            }
        }
        break;
    }
}

void UpdateProjectiles(entt::registry &registry, float deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime < 0)
        return;
    entt::entity player = entt::null;
    for (auto entity : registry.view<Player, Transform, Health>(entt::exclude<ToDestroy>)) {
        if (registry.get<Health>(entity).current > 0) {
            player = entity;
            break;
        }
    }
    for (auto entity : registry.view<Projectile, Transform>(entt::exclude<ToDestroy>)) {
        auto &shot = registry.get<Projectile>(entity);
        if (player == entt::null || shot.remaining <= 0) {
            registry.emplace<ToDestroy>(entity);
            continue;
        }
        auto &position = registry.get<Transform>(entity).matrix.row4;
        const float stepTime = (std::min)(deltaTime, shot.remaining);
        const float dx = shot.velocity.x * stepTime, dz = shot.velocity.z * stepTime;
        const auto target = registry.get<Transform>(player).matrix.row4;
        // Test the whole movement segment so fast shots cannot skip across the player.
        const float lengthSquared = dx * dx + dz * dz;
        const float t = lengthSquared > 0 ?
            (std::clamp)(((target.x - position.x) * dx + (target.z - position.z) * dz) /
                         lengthSquared, 0.0f, 1.0f) : 0.0f;
        const float x = position.x + t * dx - target.x;
        const float z = position.z + t * dz - target.z;
        position.x += dx;
        position.z += dz;
        shot.remaining -= stepTime;
        if (x * x + z * z <= shot.hitRadius * shot.hitRadius)
        {
            auto& health = registry.get<Health>(player);

            // Start with the projectile's normal damage.
            float finalDamage = shot.damage;

            // Check if the player currently has a defense potion active.
            if (registry.all_of<DefenseBoost>(player))
            {
                const auto& defense = registry.get<DefenseBoost>(player);

                // Reduce incoming damage by 10% while the potion is active.
                if (defense.remaining > 0.0f)
                {
                    finalDamage *= (1.0f - defense.damageReduction);
                }
            }

            // Apply the damage after checking the defense boost.
            health.current = (std::max)(0.0f, health.current - finalDamage);

            // Show the damage flash.
            ActivateDamageFlash(registry, player);

            // Print the actual damage dealt.
            std::cout << "Projectile hit Player: " << finalDamage
                << " damage. Health: " << health.current
                << " / " << health.maximum << std::endl;

            // Remove the projectile after it hits.
            registry.emplace<ToDestroy>(entity);

            // Stop targeting the player if they died.
            if (health.current <= 0)
                player = entt::null;
        
        } else if (shot.remaining <= 0)
            registry.emplace<ToDestroy>(entity);
    }
    HandleEntityDeath(registry);
}
void UpdateSpellAreas(entt::registry &registry, float deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime < 0)
        return;
    for (auto area : registry.view<SpellArea, DRAW::MeshCollection>(entt::exclude<ToDestroy>)) {
        auto &spell = registry.get<SpellArea>(area);
        bool livingPlayer = false;
        for (auto player : registry.view<Player, Transform, Health>(entt::exclude<ToDestroy>))
            livingPlayer |= registry.get<Health>(player).current > 0;
        if (!livingPlayer) {
            registry.emplace<ToDestroy>(area);
            continue;
        }
        spell.remaining -= deltaTime;
        if (spell.remaining > 0)
            continue;
        if (spell.detonated) {
            registry.emplace<ToDestroy>(area);
            continue;
        }
        spell.detonated = true;
        // Keep a short bright flash; damage is applied only on this transition.
        spell.remaining = 0.2f;
        for (auto mesh : registry.get<DRAW::MeshCollection>(area).meshEntities)
            if (registry.valid(mesh))
                registry.get<DRAW::GPUInstance>(mesh).matData.Ke = {1.0f, 0.5f, 1.0f};
        for (auto player : registry.view<Player, Transform, Health>(entt::exclude<ToDestroy>)) {
            auto &health = registry.get<Health>(player);
            const auto position = registry.get<Transform>(player).matrix.row4;
            const float x = position.x - spell.center.x, z = position.z - spell.center.z;
            if (health.current <= 0 || x * x + z * z > spell.radius * spell.radius)
                continue;
            // Start with the wizard's normal spell damage.
            float finalDamage = spell.damage;

            // Check if the player has an active defense potion.
            if (registry.all_of<DefenseBoost>(player))
            {
                const auto& defense = registry.get<DefenseBoost>(player);

                // Reduce spell damage while the defense potion is active.
                if (defense.remaining > 0.0f)
                {
                    finalDamage *= (1.0f - defense.damageReduction);
                }
            }

            // Apply the reduced damage to the player.
            health.current = (std::max)(0.0f, health.current - finalDamage);

            // Flash the player when they take damage.
            ActivateDamageFlash(registry, player);

            // Print the damage after applying defense.
            std::cout << "Wizard AOE hit Player: " << finalDamage
                << " damage. Health: " << health.current
                << " / " << health.maximum << std::endl;
        }
    }
    HandleEntityDeath(registry);
}
} // namespace GAME
