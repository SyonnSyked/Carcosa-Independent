#include "GameplaySystems.h"
#include "../DRAW/DrawComponents.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace GAME {
namespace {
struct Point {
    double x, z;
    Point operator+(Point b) const { return {x + b.x, z + b.z}; }
    Point operator-(Point b) const { return {x - b.x, z - b.z}; }
    Point operator*(double s) const { return {x * s, z * s}; }
};
double Dot2(Point a, Point b) { return a.x * b.x + a.z * b.z; }
double Cross2(Point a, Point b) { return a.x * b.z - a.z * b.x; }
using Footprint = std::vector<Point>;
// Numerical separation, not a content/tuning value. Bounded contact passes discard
// unresolved movement rather than letting a long frame pass through another wall.
constexpr double separation = 1e-4;
constexpr int contactPasses = 16;

Footprint WorldFootprint(const DRAW::MeshCollection &meshes, const Transform &transform)
{
    const auto &box = meshes.collider;
    GW::MATH::GMATRIXF rotation;
    GW::MATH::GMatrix::ConvertQuaternionF(box.rotation, rotation);
    Footprint points;
    for (int i = 0; i < 8; ++i) {
        GW::MATH::GVECTORF corner{(i & 1 ? 1 : -1) * box.extent.x,
                                  (i & 2 ? 1 : -1) * box.extent.y,
                                  (i & 4 ? 1 : -1) * box.extent.z, 0};
        GW::MATH::GMatrix::VectorXMatrixF(rotation, corner, corner);
        corner = {corner.x + box.center.x, corner.y + box.center.y,
                  corner.z + box.center.z, 1};
        GW::MATH::GMatrix::VectorXMatrixF(transform.matrix, corner, corner);
        points.push_back({corner.x, corner.z});
    }
    // Convex hull of the projected box also handles tilted and mirrored/scaled boxes.
    std::sort(points.begin(), points.end(), [](Point a, Point b) {
        return a.x < b.x || (a.x == b.x && a.z < b.z);
    });
    points.erase(std::unique(points.begin(), points.end(), [](Point a, Point b) {
        return a.x == b.x && a.z == b.z;
    }), points.end());
    if (points.size() < 3)
        return points;
    Footprint hull;
    for (auto p : points) {
        while (hull.size() >= 2 &&
               Cross2(hull.back() - hull[hull.size() - 2], p - hull.back()) <= 0)
            hull.pop_back();
        hull.push_back(p);
    }
    const auto lower = hull.size();
    for (int i = static_cast<int>(points.size()) - 2; i >= 0; --i) {
        const auto p = points[i];
        while (hull.size() > lower &&
               Cross2(hull.back() - hull[hull.size() - 2], p - hull.back()) <= 0)
            hull.pop_back();
        hull.push_back(p);
    }
    hull.pop_back();
    return hull;
}

// Minimal translation for an initially overlapping circle (including bad spawn placement).
Point PushOut(const Footprint &hull, Point p, double radius)
{
    bool inside = hull.size() >= 3;
    double nearest = std::numeric_limits<double>::infinity();
    Point closest{}, outward{1, 0};
    for (std::size_t i = 0; i < hull.size(); ++i) {
        const auto a = hull[i], edge = hull[(i + 1) % hull.size()] - a;
        const double length2 = Dot2(edge, edge);
        if (Cross2(edge, p - a) < 0)
            inside = false;
        const double t = length2 > 0 ? std::clamp(Dot2(p - a, edge) / length2, 0.0, 1.0) : 0;
        const auto q = a + edge * t;
        const double distance2 = Dot2(p - q, p - q);
        if (distance2 < nearest) {
            nearest = distance2;
            closest = q;
            if (length2 > 0)
                outward = Point{edge.z, -edge.x} * (1 / std::sqrt(length2));
        }
    }
    const double distance = std::sqrt(nearest);
    if (!inside && distance >= radius)
        return {};
    const auto normal = distance > 0 ? (inside ? closest - p : p - closest) * (1 / distance) : outward;
    return normal * (radius + separation + (inside ? distance : -distance));
}

struct Contact {
    double time = 1;
    Point normal{};
    bool hit = false;
};
void Sweep(const Footprint &hull, Point start, Point move, double radius, Contact &contact)
{
    auto accept = [&](double t, Point normal) {
        if (t >= 0 && t <= contact.time && Dot2(move, normal) < 0) {
            contact = {t, normal, true};
        }
    };
    for (std::size_t i = 0; i < hull.size(); ++i) {
        const auto a = hull[i], edge = hull[(i + 1) % hull.size()] - a;
        const double length2 = Dot2(edge, edge);
        if (length2 > 0) {
            const auto normal = Point{edge.z, -edge.x} * (1 / std::sqrt(length2));
            const double approach = Dot2(move, normal);
            if (approach < 0) {
                const double t = (radius - Dot2(start - a, normal)) / approach;
                const double along = Dot2(start + move * t - a, edge) / length2;
                if (along >= 0 && along <= 1)
                    accept(t, normal);
            }
        }
        // Rounded corners: sweep the center against a radius-sized circle at each vertex.
        const auto offset = start - a;
        const double aa = Dot2(move, move), b = Dot2(offset, move);
        const double c = Dot2(offset, offset) - radius * radius;
        const double discriminant = b * b - aa * c;
        if (aa > 0 && discriminant >= 0) {
            const double t = (-b - std::sqrt(discriminant)) / aa;
            const auto normal = (offset + move * t) * (1 / radius);
            accept(t, normal);
        }
    }
}
} // namespace

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
} // namespace GAME
