#include "GameManager.h"
#include "GameComponents.h"
#include "GameplaySystems.h"
#include "../DRAW/DrawComponents.h"
#include "../UTIL/Utilities.h"

namespace GAME
{
entt::entity CreateRenderable(entt::registry& registry, const std::string& modelName,
                              const GW::MATH::GMATRIXF& transform)
{
    const auto* manager = registry.ctx().find<DRAW::ModelManager>();
    if (!manager || !manager->models.contains(modelName))
        throw std::runtime_error("Renderable template not loaded: " + modelName +
                                 " (export it in the selected world with gameType != none)");
    const auto& source = manager->models.at(modelName);
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

void InitializeGameplay(entt::registry& registry)
{
    const auto& settings = *registry.ctx().get<UTIL::Config>().gameConfig;
    const auto levels = registry.view<DRAW::CPULevel>();
    const auto& level = registry.get<DRAW::CPULevel>(levels.front()).lvlData;
    for (const auto* role : {"Player", "Enemy"}) {
        const auto& tuning = settings.at(role);
        const auto name = tuning.at("model").as<std::string>();
        bool found = false;
        for (const auto& object : level.blenderObjects) {
            if (name != object.blendername) continue;
            const auto entity = CreateRenderable(registry, name, level.levelTransforms[object.transformIndex]);
            if (std::string(role) == "Player") registry.emplace<Player>(entity);
            else registry.emplace<Enemy>(entity);
            const float health = tuning.at("health").as<float>();
            registry.emplace<Health>(entity, Health{health, health});
            registry.emplace<Movement>(entity, Movement{tuning.at("speed").as<float>()});
            registry.emplace<MeleeAttack>(entity, MeleeAttack{tuning.at("damage").as<float>(),
                tuning.at("range").as<float>(), tuning.at("cooldown").as<float>()});
            found = true;
            break;
        }
        if (!found) throw std::runtime_error("Gameplay object not loaded: " + name);
    }
    UpdateCamera(registry);
    std::cout << "Gameplay ready: WASD moves, SPACE attacks (press again for each swing)." << std::endl;
}

void UpdateGameSystems(entt::registry& registry)
{
    const float deltaTime = static_cast<float>(registry.ctx().get<UTIL::DeltaTime>().dtSec);
    UpdateCombatTimers(registry, deltaTime);
    UpdatePlayer(registry, deltaTime);
    UpdateEnemy(registry, deltaTime);
    UpdateCamera(registry);

    for (auto entity : registry.view<Transform, Velocity>()) {
        auto& transform = registry.get<Transform>(entity).matrix;
        const auto& velocity = registry.get<Velocity>(entity).vector;
        transform.row4.x += velocity.x * deltaTime;
        transform.row4.y += velocity.y * deltaTime;
        transform.row4.z += velocity.z * deltaTime;
    }

    for (auto entity : registry.view<Transform, DRAW::MeshCollection>()) {
        const auto& transform = registry.get<Transform>(entity).matrix;
        const auto& meshes = registry.get<DRAW::MeshCollection>(entity).meshEntities;
        for (auto mesh : meshes)
            if (registry.valid(mesh) && registry.all_of<DRAW::GPUInstance>(mesh))
                registry.get<DRAW::GPUInstance>(mesh).transform = transform;
    }

    const auto pending = registry.view<ToDestroy>();
    std::vector<entt::entity> entities(pending.begin(), pending.end());
    for (auto entity : entities)
        if (registry.valid(entity)) registry.destroy(entity);
}
}
