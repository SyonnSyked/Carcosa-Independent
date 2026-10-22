#include "GameComponents.h"
#include "GameManager.h"
#include "GameplaySystems.h"
#include "../DRAW/DrawComponents.h"
#include "../UTIL/Utilities.h"

namespace GAME {
void InitializeDungeonKeys(entt::registry &registry)
{
    const auto &settings = *registry.ctx().get<UTIL::Config>().gameConfig;
    const auto levelText = settings.at("DungeonKey").at("level").as<std::string>();
    std::size_t consumed = 0;
    const int level = std::stoi(levelText, &consumed);
    DungeonKeyConfig config{settings.at("DungeonKey").at("model").as<std::string>(),
                            {level, settings.RequireFloat("DungeonKey", "distToUse")},
                            settings.RequireFloat("DungeonKey", "pickupRadius")};
    const auto exitModel = settings.at("DungeonExit").at("model").as<std::string>();
    if (consumed != levelText.size() || level <= 0 || config.model.empty() ||
        exitModel.empty() || config.key.distToUse <= 0 || config.pickupRadius <= 0)
        throw std::runtime_error("Invalid DungeonKey/DungeonExit configuration: require model names, positive integer level and positive distances");
    registry.ctx().emplace<DungeonKeyConfig>(config);
    registry.ctx().emplace<DungeonState>(DungeonState{level});

    const auto *models = registry.ctx().find<DRAW::ModelManager>();
    if (!models || !models->models.contains(config.model) ||
        models->models.at(config.model).meshEntities.empty())
        std::cout << "Dungeon Key template unavailable: export " << config.model
                  << ".h2b to the configured models folder and include the named mesh in the selected level with gameType=key.\n";

    // The loader hides the dynamic template; this is the only visible door instance.
    for (auto entity : registry.view<DRAW::CPULevel>()) {
        const auto &data = registry.get<DRAW::CPULevel>(entity).lvlData;
        for (const auto &object : data.blenderObjects) {
            if (exitModel != object.blendername)
                continue;
            const Transform placement{data.levelTransforms.at(object.transformIndex)};
            const auto door = CreateRenderable(registry, exitModel, placement.matrix);
            registry.emplace<DungeonExit>(door, DungeonExit{placement});
            return;
        }
    }
    throw std::runtime_error("DungeonExit object unavailable: export " + exitModel +
                             ".h2b and its level object with gameType=exit");
}

void DropDungeonKey(entt::registry &registry, const Transform placement)
{
    const auto &config = registry.ctx().get<DungeonKeyConfig>();
    const auto *models = registry.ctx().find<DRAW::ModelManager>();
    if (!models || !models->models.contains(config.model) ||
        models->models.at(config.model).meshEntities.empty()) {
        std::cout << "Dungeon Key drop failed: export " << config.model
                  << ".h2b and its named level mesh with gameType=key; no key was spawned.\n";
        return;
    }
    const auto dungeonKey = CreateRenderable(registry, config.model, placement.matrix);
    registry.emplace<DungeonKey>(dungeonKey, config.key);
}

void CollectDungeonKey(entt::registry &registry)
{
    for (auto player : registry.view<Player, Transform, Health>(entt::exclude<ToDestroy>)) {
        if (registry.get<Health>(player).current <= 0 || registry.all_of<DungeonKey>(player))
            continue;
        for (auto item : registry.view<DungeonKey, Transform, DRAW::MeshCollection>(
                 entt::exclude<Player, ToDestroy>)) {
            if (GroundDistance(registry, item, player) >
                registry.ctx().get<DungeonKeyConfig>().pickupRadius)
                continue;
            const auto key = registry.get<DungeonKey>(item);
            registry.emplace<DungeonKey>(player, key);
            registry.emplace<ToDestroy>(item);
            break; // One key per player; pending world keys cannot be collected again.
        }
    }
}
void UseDungeonKey(entt::registry &registry)
{
    auto &state = registry.ctx().get<DungeonState>();
    if (state.complete)
        return;
    const auto player = registry.view<Player, Transform, DungeonKey, Health>(
        entt::exclude<ToDestroy>).front();
    const auto door = registry.view<DungeonExit, Transform>(entt::exclude<ToDestroy>).front();
    if (player == entt::null || door == entt::null ||
        registry.get<Health>(player).current <= 0)
        return;
    const auto &key = registry.get<DungeonKey>(player);
    if (key.level != state.level || GroundDistance(registry, door, player) > key.distToUse)
        return;




    registry.remove<DungeonKey>(player);

        if(state.level == 1)
        {
            state.level = 2;
            std::cout << "Level 1 completed moving to level 2";
            LoadLevel(registry, "level2");

        }
else{

      state.complete = true;
    ChangeScreen(Screens::WinMenu);
    std::cout << "Dungeon " << state.level << " COMPLETE: key consumed, win screen opened."
              << std::endl;
}



    // Completion freezes gameplay while the win screen offers the next action.
  
}
} // namespace GAME
