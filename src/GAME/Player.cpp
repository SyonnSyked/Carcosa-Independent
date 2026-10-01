#include "GameComponents.h"
#include "GameplaySystems.h"
#include "../UTIL/Utilities.h"
#include "../DRAW/DrawComponents.h"

namespace GAME {
void UpdatePlayer(entt::registry& registry, float deltaTime)
{
    auto& input = registry.ctx().get<UTIL::Input>().immediateInput;
    const auto down = [&](int key) {
        float value = 0;
        input.GetState(key, value);
        return value > 0;
    };
    for (auto entity : registry.view<Player, Transform, Movement, Health>()) {
        auto& player = registry.get<Player>(entity);
        const bool attackDown = down(G_KEY_SPACE);
        const bool pressed = attackDown && !player.attackHeld;
        player.attackHeld = attackDown;
        if (registry.get<Health>(entity).current <= 0) continue;
        float x = float(down(G_KEY_D)) - float(down(G_KEY_A));
        float z = float(down(G_KEY_W)) - float(down(G_KEY_S));
        const float length = std::sqrt(x*x + z*z);
        if (length > 0) {
            const float step = registry.get<Movement>(entity).speed * deltaTime / length;
            // Gameplay edits only the ECS transform; GameManager synchronizes owned meshes.
            auto& position = registry.get<Transform>(entity).matrix.row4;
            position.x += x * step;
            position.z += z * step;
        }
        if (pressed)
            for (auto enemy : registry.view<Enemy, Transform, Health>())
                TryMeleeAttack(registry, entity, enemy);
    }
}

void UpdateCamera(entt::registry& registry)
{
    const auto& settings = registry.ctx().get<UTIL::Config>().gameConfig->at("GameplayCamera");
    for (auto player : registry.view<Player, Transform>()) {
        const auto target = registry.get<Transform>(player).matrix.row4;
        const GW::MATH::GVECTORF eye{target.x + settings.at("x").as<float>(),
            target.y + settings.at("y").as<float>(), target.z + settings.at("z").as<float>(), 1};
        GW::MATH::GMATRIXF view;
        GW::MATH::GMatrix::LookAtLHF(eye, target, {0, 1, 0, 0}, view);
        // DRAW::Camera stores a world matrix; the renderer already inverts it for the view.
        for (auto camera : registry.view<DRAW::Camera>())
            GW::MATH::GMatrix::InverseF(view, registry.get<DRAW::Camera>(camera).camMatrix);
        break;
    }
}
}
