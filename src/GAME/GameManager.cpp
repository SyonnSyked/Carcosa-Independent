#include "GameManager.h"
#include "GameComponents.h"
#include "GameplaySystems.h"
#include "../DRAW/DrawComponents.h"
#include "../UTIL/Utilities.h"

namespace GAME {
namespace {
void AttachCharacter(entt::registry &registry, entt::entity entity, const CharacterConfig &config)
{
    registry.emplace<Health>(entity, Health{config.health, config.health});
    registry.emplace<Movement>(entity, Movement{config.speed});
    auto attack = config.attackData;
    attack.remaining = 0;
    registry.emplace<MeleeAttack>(entity, attack);
}

CharacterConfig LoadCharacter(entt::registry &registry, const char *role)
{
    const auto &tuning = registry.ctx().get<UTIL::Config>().gameConfig->at(role);
    CharacterConfig config;
    config.model = tuning.at("model").as<std::string>();
    config.health = tuning.at("health").as<float>();
    config.speed = tuning.at("speed").as<float>();
    config.attackData = {tuning.at("damage").as<float>(), tuning.at("range").as<float>(),
                         tuning.at("cooldown").as<float>()};
    if (!std::isfinite(config.health) || config.health <= 0 ||
        !std::isfinite(config.speed) || config.speed < 0 ||
        !std::isfinite(config.attackData.damage) || config.attackData.damage < 0 ||
        !std::isfinite(config.attackData.range) || config.attackData.range < 0 ||
        !std::isfinite(config.attackData.cooldown) || config.attackData.cooldown < 0)
        throw std::runtime_error(std::string("Invalid character tuning: ") + role);
    const auto *manager = registry.ctx().find<DRAW::ModelManager>();
    if (!manager || !manager->models.contains(config.model) ||
        manager->models.at(config.model).meshEntities.empty())
        throw std::runtime_error("Gameplay renderable template not loaded: " + config.model);
    const auto levels = registry.view<DRAW::CPULevel>();
    if (!levels.empty()) {
        const auto &level = registry.get<DRAW::CPULevel>(levels.front()).lvlData;
        for (const auto &object : level.blenderObjects) {
            if (config.model == object.blendername) {
                config.transform = {level.levelTransforms.at(object.transformIndex)};
                return config;
            }
        }
    }
    throw std::runtime_error("Gameplay object not loaded: " + config.model);
}
} // namespace

entt::entity CreateRenderable(entt::registry &registry, const std::string &modelName,
                              const GW::MATH::GMATRIXF &transform)
{
    const auto *manager = registry.ctx().find<DRAW::ModelManager>();
    if (!manager || !manager->models.contains(modelName))
        throw std::runtime_error("Renderable template not loaded: " + modelName +
                                 " (export it in the selected world with gameType != none)");
    const auto &source = manager->models.at(modelName);
    for (auto mesh : source.meshEntities)
        if (!registry.valid(mesh) || !registry.all_of<DRAW::GeometryData, DRAW::GPUInstance>(mesh))
            throw std::runtime_error("Renderable template is no longer valid: " + modelName);

    const auto entity = registry.create();
    try {
        registry.emplace<Transform>(entity, Transform{transform});
        registry.emplace<DRAW::MeshCollection>(entity, DRAW::MeshCollection{{}, source.collider});
        for (auto mesh : source.meshEntities) {
            const auto geometry = registry.get<DRAW::GeometryData>(mesh);
            auto instance = registry.get<DRAW::GPUInstance>(mesh);
            instance.transform = transform;
            const auto clone = registry.create();
            registry.get<DRAW::MeshCollection>(entity).meshEntities.push_back(clone);
            registry.emplace<DRAW::GeometryData>(clone, geometry);
            registry.emplace<DRAW::GPUInstance>(clone, instance);
        }
    } catch (...) {
        registry.destroy(entity);
        throw;
    }
    return entity;
}

void InitializeGameplay(entt::registry &registry)
{
    const auto &settings = *registry.ctx().get<UTIL::Config>().gameConfig;
    const auto playerConfig = LoadCharacter(registry, "Player");
    const auto enemyConfig = LoadCharacter(registry, "Enemy");
    const auto &tuning = settings.at("HordeSpawner");
    HordeSpawner spawner{tuning.at("enabled").as<bool>(),
                         tuning.at("spawnInterval").as<float>(),
                         tuning.at("minimumRadius").as<float>(),
                         tuning.at("maximumRadius").as<float>(),
                         tuning.at("maximumAlive").as<int>(),
                         tuning.at("totalSpawnLimit").as<int>()};
    if (!std::isfinite(spawner.spawnInterval) || spawner.spawnInterval < 0 ||
        !std::isfinite(spawner.minimumRadius) || spawner.minimumRadius < 0 ||
        !std::isfinite(spawner.maximumRadius) || spawner.minimumRadius > spawner.maximumRadius ||
        spawner.maximumAlive <= 0 || spawner.totalSpawnLimit < 0)
        throw std::runtime_error("Invalid HordeSpawner configuration: check interval, radii and population limits");

    // The shared recipe is parsed once. Each actor still owns independent ECS state.
    InitializeDamageFeedback(registry);
    registry.ctx().emplace<CharacterConfig>(enemyConfig);
    const auto player = CreateRenderable(registry, playerConfig.model, playerConfig.transform.matrix);
    registry.emplace<Player>(player);
    AttachCharacter(registry, player, playerConfig);
    CreateEnemy(registry, enemyConfig, enemyConfig.transform.matrix);
    const auto spawnerEntity = registry.create();
    registry.emplace<HordeSpawner>(spawnerEntity, spawner);
    registry.emplace<Transform>(spawnerEntity, enemyConfig.transform);
    UpdateCamera(registry);
    std::cout << "Gameplay ready: WASD moves, SPACE attacks (press again for each swing)."
              << std::endl;
}

void UpdateGameSystems(entt::registry &registry)
{
    const float deltaTime = static_cast<float>(registry.ctx().get<UTIL::DeltaTime>().dtSec);
    // Age previous flashes first; hits this frame get their full configured duration.
    UpdateDamageFlash(registry, deltaTime);
    UpdateCombatTimers(registry, deltaTime);
    UpdatePlayer(registry, deltaTime);
    UpdateEnemy(registry, deltaTime);
    // Combat deaths free population slots now; new enemies begin AI next frame.
    // Spawn before mesh synchronization so new instances render at their placement immediately.
    UpdateHordeSpawner(registry, deltaTime);
    UpdateCamera(registry);

    for (auto entity : registry.view<Transform, Velocity>()) {
        auto &transform = registry.get<Transform>(entity).matrix;
        const auto &velocity = registry.get<Velocity>(entity).vector;
        transform.row4.x += velocity.x * deltaTime;
        transform.row4.y += velocity.y * deltaTime;
        transform.row4.z += velocity.z * deltaTime;
    }

    for (auto entity : registry.view<Transform, DRAW::MeshCollection>()) {
        const auto &transform = registry.get<Transform>(entity).matrix;
        const auto &meshes = registry.get<DRAW::MeshCollection>(entity).meshEntities;
        for (auto mesh : meshes)
            if (registry.valid(mesh) && registry.all_of<DRAW::GPUInstance>(mesh))
                registry.get<DRAW::GPUInstance>(mesh).transform = transform;
    }

    const auto pending = registry.view<ToDestroy>();
    std::vector<entt::entity> entities(pending.begin(), pending.end());
    for (auto entity : entities)
        if (registry.valid(entity))
            registry.destroy(entity);
}

entt::entity CreateEnemy(entt::registry &registry, const CharacterConfig &config,
                         const GW::MATH::GMATRIXF &placement)
{
    const auto enemy = CreateRenderable(registry, config.model, placement);
    try {
        registry.emplace<Enemy>(enemy);
        AttachCharacter(registry, enemy, config);
    } catch (...) {
        registry.destroy(enemy); // Existing MeshCollection callback owns mesh cleanup.
        throw;
    }
    return enemy;
}

} // namespace GAME
