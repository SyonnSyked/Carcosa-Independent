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

Footprint WorldFootprint(const DRAW::MeshCollection &meshes, const Transform &transform);


// Minimal translation for an initially overlapping circle (including bad spawn placement).
Point PushOut(const Footprint &hull, Point p, double radius);


struct Contact {
    double time = 1;
    Point normal{};
    bool hit = false;
};
void Sweep(const Footprint &hull, Point start, Point move, double radius, Contact &contact);



void MovePlayerWithCollision(entt::registry &registry, entt::entity player, float x, float z);

} // namespace GAME
