#include "GameComponents.h"
#include "GameManager.h"
#include "GameplaySystems.h"
#include <numbers>
#include <random>

namespace GAME {
namespace {
entt::entity FindLivingSpawnPlayer(entt::registry &registry)
{
    for (auto entity : registry.view<Player, Transform, Health>(entt::exclude<ToDestroy>)) {
        if (registry.get<Health>(entity).current > 0)
            return entity;
    }
    return entt::null;
}

int CountLivingEnemies(entt::registry &registry)
{
    int alive = 0;
    for (auto enemy : registry.view<Enemy, Health>(entt::exclude<ToDestroy>))
        if (registry.get<Health>(enemy).current > 0)
            ++alive;
    return alive;
}
} // namespace

void UpdateHordeSpawner(entt::registry &registry, float deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime < 0)
        return;
    const auto player = FindLivingSpawnPlayer(registry);
    if (player == entt::null)
        return;
    int alive = CountLivingEnemies(registry);

    // One generator per registry; never reseed per enemy.
    auto *random = registry.ctx().find<std::mt19937>();
    if (!random)
        random = &registry.ctx().emplace<std::mt19937>(std::random_device{}());
    const auto &config = registry.ctx().get<CharacterConfig>();
    const auto center = registry.get<Transform>(player).matrix.row4;
    for (auto entity : registry.view<HordeSpawner, Transform>(entt::exclude<ToDestroy>)) {
        auto &spawner = registry.get<HordeSpawner>(entity);
        if (!spawner.enabled ||
            (spawner.totalSpawnLimit != 0 && spawner.spawned >= spawner.totalSpawnLimit))
            continue;
        // Saturate instead of accumulating a backlog while capped or after a long frame.
        spawner.elapsed = static_cast<float>(
            (std::min)(double(spawner.spawnInterval), double(spawner.elapsed) + deltaTime));
        if (spawner.elapsed < spawner.spawnInterval || alive >= spawner.maximumAlive)
            continue;
        std::uniform_real_distribution<float> angle(0, 2 * std::numbers::pi_v<float>);
        std::uniform_real_distribution<float> radius(spawner.minimumRadius, spawner.maximumRadius);
        const float direction = angle(*random);
        const float distance = radius(*random);
        // Retain exported orientation, scale and Y height; replace only X/Z placement.
        auto placement = registry.get<Transform>(entity).matrix;
        placement.row4.x = center.x + std::cos(direction) * distance;
        placement.row4.z = center.z + std::sin(direction) * distance;
        std::discrete_distribution<int> variant{
            spawner.meleeWeight, spawner.rangedWeight, spawner.wizardWeight};
        CreateEnemyVariant(registry, config, placement,
                           static_cast<EnemyVariant>(variant(*random)));
        ++alive;
        ++spawner.spawned;
        spawner.elapsed = 0; // At most one spawn per spawner/frame, including interval zero.
    }
}

} // namespace GAME
