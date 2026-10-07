#include "../DRAW/DrawComponents.h"
#include "GameManager.h"
#include "GameComponents.h"
#include "GameplaySystems.h"
#include <algorithm>

namespace GAME {
namespace {
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
    for (auto player : registry.view<Player, Transform, Health>()) {
        if (registry.get<Health>(player).current <= 0)
            return;
        for (auto enemy : registry.view<Enemy, Transform, Movement, MeleeAttack, Health>(entt::exclude<ToDestroy>)) {
            if (registry.get<Health>(enemy).current <= 0)
                continue;
            const float distance = GroundDistance(registry, enemy, player);
            auto *ranged = registry.try_get<RangedAttack>(enemy);
            if (ranged)
                ranged->remaining = (std::max)(0.0f, ranged->remaining - deltaTime);
            const float range = ranged ? ranged->range : registry.get<MeleeAttack>(enemy).range;
            if (distance > range) {
                const auto &target = registry.get<Transform>(player).matrix.row4;
                auto &position = registry.get<Transform>(enemy).matrix.row4;
                // Stop at the attack range, even on a long frame. Preserve exported ground height.
                const float step =
                    (std::min)(registry.get<Movement>(enemy).speed * deltaTime, distance - range);
                position.x += (target.x - position.x) / distance * step;
                position.z += (target.z - position.z) / distance * step;
            } else {
                if (ranged) {
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
        if (x * x + z * z <= shot.hitRadius * shot.hitRadius) {
            auto &health = registry.get<Health>(player);
            health.current = (std::max)(0.0f, health.current - shot.damage);
            ActivateDamageFlash(registry, player);
            std::cout << "Projectile hit Player: " << shot.damage << " damage. Health: "
                      << health.current << " / " << health.maximum << std::endl;
            registry.emplace<ToDestroy>(entity);
            if (health.current <= 0)
                player = entt::null;
        } else if (shot.remaining <= 0)
            registry.emplace<ToDestroy>(entity);
    }
}
} // namespace GAME
