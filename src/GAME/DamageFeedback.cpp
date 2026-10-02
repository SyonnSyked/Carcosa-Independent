#include "../DRAW/DrawComponents.h"
#include "../UTIL/Utilities.h"
#include "GameComponents.h"
#include "GameplaySystems.h"

namespace GAME {
namespace {
// Only the modified RGB is saved, on the owned mesh itself; no resources or entity references.
struct NormalEmission {
    H2B::VECTOR color;
};

void SetFlashAppearance(entt::registry &registry, entt::entity entity, bool active)
{
    const auto *meshes = registry.try_get<DRAW::MeshCollection>(entity);
    if (!meshes)
        return;
    const auto &config = registry.ctx().get<DamageFeedbackConfig>();
    for (auto mesh : meshes->meshEntities) {
        if (!registry.valid(mesh))
            continue;
        auto *instance = registry.try_get<DRAW::GPUInstance>(mesh);
        if (!instance)
            continue;
        auto *normal = registry.try_get<NormalEmission>(mesh);
        if (active) {
            // Repeated hits must not save the flashed color as the original.
            if (!normal)
                registry.emplace<NormalEmission>(mesh, NormalEmission{instance->matData.Ke});
            instance->matData.Ke = {config.flashR, config.flashG, config.flashB};
        } else if (normal) {
            instance->matData.Ke = normal->color;
            registry.remove<NormalEmission>(mesh);
        }
    }
}
} // namespace

void InitializeDamageFeedback(entt::registry &registry)
{
    const auto &settings = *registry.ctx().get<UTIL::Config>().gameConfig;
    DamageFeedbackConfig config{settings.at("DamageFeedback").at("enabled").as<bool>(),
                                settings.RequireFloat("DamageFeedback", "duration"),
                                settings.RequireFloat("DamageFeedback", "flashR"),
                                settings.RequireFloat("DamageFeedback", "flashG"),
                                settings.RequireFloat("DamageFeedback", "flashB")};
    const auto validColor = [](float channel) {
        return std::isfinite(channel) && channel >= 0 && channel <= 1;
    };
    if (!std::isfinite(config.duration) || config.duration <= 0 || !validColor(config.flashR) ||
        !validColor(config.flashG) || !validColor(config.flashB))
        throw std::runtime_error("Invalid DamageFeedback configuration: duration must be positive "
                                 "and finite; RGB must be in [0,1]");
    registry.ctx().emplace<DamageFeedbackConfig>(config);
}

void ActivateDamageFlash(entt::registry &registry, entt::entity entity)
{
    const auto &config = registry.ctx().get<DamageFeedbackConfig>();
    if (!config.enabled || !registry.all_of<DRAW::MeshCollection>(entity))
        return;
    registry.emplace_or_replace<DamageFlash>(entity, DamageFlash{config.duration});
    SetFlashAppearance(registry, entity, true);
}

void UpdateDamageFlash(entt::registry &registry, float deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime < 0)
        return;
    for (auto entity : registry.view<DamageFlash>()) {
        auto &flash = registry.get<DamageFlash>(entity);
        flash.remaining = (std::max)(0.0f, flash.remaining - deltaTime);
        const bool active =
            registry.ctx().get<DamageFeedbackConfig>().enabled && flash.remaining > 0;
        SetFlashAppearance(registry, entity, active);
        if (!active)
            registry.remove<DamageFlash>(entity);
    }
}
} // namespace GAME
