#include "GameplaySystems.h"
#include "../DRAW/DrawComponents.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "Collision.h"

namespace GAME {


  






void MovePlayerWithCollision(entt::registry &registry, entt::entity player, float x, float z)
{
    auto &position = registry.get<Transform>(player).matrix.row4;
    const double radius = registry.ctx().get<CollisionConfig>().playerRadius;
    Point current{position.x, position.z}, remaining{x, z};
    // Per-call footprints only: ECS remains the sole source of obstacle ownership/data.
    std::vector<Footprint> obstacles;
    for (auto obstacle : registry.view<Collidable, Obstacle, Transform, DRAW::MeshCollection>(
             entt::exclude<ToDestroy>))
        obstacles.push_back(WorldFootprint(registry.get<DRAW::MeshCollection>(obstacle),
                                          registry.get<Transform>(obstacle)));

    bool clear = false;
    for (int pass = 0; pass < contactPasses; ++pass) {
        clear = true;
        for (const auto &hull : obstacles) {
            const auto correction = PushOut(hull, current, radius + separation);
            if (Dot2(correction, correction) > 0) {
                current = current + correction;
                clear = false;
            }
        }
        if (clear)
            break;
    }
    if (!clear)
        throw std::runtime_error("Player collision could not resolve overlapping obstacles: check static collider bounds and player spawn placement");

    for (int pass = 0; pass < contactPasses && Dot2(remaining, remaining) > 0; ++pass) {
        Contact contact;
        for (const auto &hull : obstacles)
            Sweep(hull, current, remaining, radius + separation, contact);
        if (!contact.hit) {
            current = current + remaining;
            break;
        }
        current = current + remaining * contact.time;
        remaining = remaining * (1 - contact.time);
        // Remove only the inward component, preserving movement tangent to rotated walls.
        remaining = remaining - contact.normal * Dot2(remaining, contact.normal);
    }
    position.x = static_cast<float>(current.x);
    position.z = static_cast<float>(current.z);
}


}