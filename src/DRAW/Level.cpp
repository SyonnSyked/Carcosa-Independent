#include "../CCL.h"
#include "DrawComponents.h"
#include "VulkanTextures.h"
#include "../GAME/GameComponents.h"

namespace DRAW {
void Construct_CPULevel(entt::registry &registry, entt::entity entity)
{
    auto &cpuLevel = registry.get<CPULevel>(entity);

    GW::SYSTEM::GLog log;
    log.Create("LevelLoaderLog.txt");
    log.EnableConsoleLogging(true);

    if (!cpuLevel.lvlData.LoadLevel(cpuLevel.levelPath.c_str(), cpuLevel.modelFolder.c_str(), log))
        throw std::runtime_error("Could not load world: " + cpuLevel.levelPath);
}

void Construct_GPULevel(entt::registry &registry, entt::entity entity)
{
    auto *cpuLevel = registry.try_get<CPULevel>(entity);

    if (cpuLevel == nullptr)
        return;

    auto &modelManager = registry.ctx().emplace<ModelManager>();
    if (cpuLevel->lvlData.levelVertices.empty() || cpuLevel->lvlData.levelIndices.empty()) {
        std::cout << "World contains no renderable geometry. Export JSON to "
                  << cpuLevel->levelPath << " and H2B models to " << cpuLevel->modelFolder << '\n';
        return;
    }

    registry.emplace<VulkanVertexBuffer>(entity);
    registry.emplace<std::vector<H2B::VERTEX>>(entity, cpuLevel->lvlData.levelVertices);
    registry.patch<VulkanVertexBuffer>(entity);

    registry.emplace<VulkanIndexBuffer>(entity);
    registry.emplace<std::vector<unsigned int>>(entity, cpuLevel->lvlData.levelIndices);
    registry.patch<VulkanIndexBuffer>(entity);

    auto &models = modelManager.models;
    auto &renderer = registry.get<VulkanRenderer>(entity);

    for (const auto &object : cpuLevel->lvlData.blenderObjects) {
        const auto &model = cpuLevel->lvlData.levelModels[object.modelIndex];
        const auto &transform = cpuLevel->lvlData.levelTransforms[object.transformIndex];
        MeshCollection meshCollection;
        meshCollection.collider = cpuLevel->lvlData.levelColliders[model.colliderIndex];

        if (model.isCollidable && !model.isDynamic) {
            auto obstacle = registry.create();
            registry.emplace<GAME::Collidable>(obstacle);
            registry.emplace<GAME::Obstacle>(obstacle);
            registry.emplace<GAME::Transform>(obstacle, GAME::Transform{transform});
            registry.emplace<MeshCollection>(obstacle,
                                             MeshCollection{{}, meshCollection.collider});
        }

        for (unsigned int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
            const auto &mesh = cpuLevel->lvlData.levelMeshes[model.meshStart + meshIndex];
            const auto &material =
                cpuLevel->lvlData.levelMaterials[model.materialStart + mesh.materialIndex];
            auto meshEntity = registry.create();

            registry.emplace<GeometryData>(
                meshEntity,
                GeometryData{model.indexStart + mesh.drawInfo.indexOffset,
                             mesh.drawInfo.indexCount,
                             model.vertexStart, LoadDiffuseTexture(renderer, material)});
            registry.emplace<GPUInstance>(meshEntity, GPUInstance{transform, material.attrib});
            if (model.isDynamic)
                registry.emplace<DoNotRender>(meshEntity);
            if (model.isDynamic)
                meshCollection.meshEntities.push_back(meshEntity);
        }

        if (model.isDynamic)
            models.emplace(object.blendername, std::move(meshCollection));
    }
}

void Destroy_MeshCollection(entt::registry &registry, entt::entity entity)
{
    auto &meshCollection = registry.get<MeshCollection>(entity);
    for (auto meshEntity : meshCollection.meshEntities) {
        if (registry.valid(meshEntity))
            registry.destroy(meshEntity);
    }
}

CONNECT_COMPONENT_LOGIC()
{
    registry.on_construct<CPULevel>().connect<Construct_CPULevel>();
    registry.on_construct<GPULevel>().connect<Construct_GPULevel>();
    registry.on_destroy<MeshCollection>().connect<Destroy_MeshCollection>();
}
}
