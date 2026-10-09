#include "../DRAW/DrawComponents.h"
#include "../UTIL/Utilities.h"
#include "GameComponents.h"
#include "GameplaySystems.h"

namespace GAME {
namespace {
using GW::MATH::GVECTORF;
GVECTORF Subtract(GVECTORF a, GVECTORF b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z, 0};
}
GVECTORF Cross(GVECTORF a, GVECTORF b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x, 0};
}
float Dot(GVECTORF a, GVECTORF b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Segment parameter remains comparable between differently scaled mesh instances.
bool IntersectTriangle(GVECTORF origin, GVECTORF direction, GVECTORF a, GVECTORF b, GVECTORF c,
                       float &distance)
{
    const auto ab = Subtract(b, a), ac = Subtract(c, a);
    const auto p = Cross(direction, ac);
    const float determinant = Dot(ab, p);
    if (std::abs(determinant) < 1e-7f)
        return false;
    const auto offset = Subtract(origin, a);
    const float u = Dot(offset, p) / determinant;
    if (u < 0 || u > 1)
        return false;
    const auto q = Cross(offset, ab);
    const float v = Dot(direction, q) / determinant;
    if (v < 0 || u + v > 1)
        return false;
    distance = Dot(ac, q) / determinant;
    return distance >= 0 && distance <= 1;
}

bool IntersectMesh(entt::registry &registry, entt::entity mesh, const Level_Data &level,
                   GVECTORF nearPoint, GVECTORF farPoint, float &nearest)
{
    GW::MATH::GMATRIXF inverseWorld;
    if (-GW::MATH::GMatrix::InverseF(registry.get<DRAW::GPUInstance>(mesh).transform, inverseWorld))
        return false;
    GVECTORF origin, end;
    GW::MATH::GMatrix::VectorXMatrixF(inverseWorld, nearPoint, origin);
    GW::MATH::GMatrix::VectorXMatrixF(inverseWorld, farPoint, end);
    const auto direction = Subtract(end, origin);
    const auto &geometry = registry.get<DRAW::GeometryData>(mesh);
    auto vertex = [&](unsigned int index) {
        const auto &position =
            level.levelVertices.at(geometry.vertexStart + level.levelIndices.at(index)).pos;
        return GVECTORF{position.x, position.y, position.z, 1};
    };
    bool hit = false;
    nearest = 2;
    for (unsigned int i = 0; i + 2 < geometry.indexCount; i += 3) {
        float distance;
        const auto start = geometry.indexStart + i;
        if (IntersectTriangle(origin, direction, vertex(start), vertex(start + 1),
                              vertex(start + 2), distance) &&
            distance < nearest) {
            nearest = distance;
            hit = true;
        }
    }
    return hit;
}

entt::entity PickEnemyAlongRay(entt::registry &registry, const Level_Data &level,
                               GVECTORF nearPoint, GVECTORF farPoint)
{
    std::unordered_map<entt::entity, entt::entity> owners;
    for (auto actor : registry.view<Enemy, DRAW::MeshCollection>())
        for (auto mesh : registry.get<DRAW::MeshCollection>(actor).meshEntities)
            owners.emplace(mesh, actor);

    float nearest = 2;
    entt::entity nearestMesh = entt::null, target = entt::null;
    // Include scenery and other actors as occluders, excluding hidden render templates.
    for (auto mesh :
         registry.view<DRAW::GeometryData, DRAW::GPUInstance>(entt::exclude<DRAW::DoNotRender>)) {
        float distance;
        if (IntersectMesh(registry, mesh, level, nearPoint, farPoint, distance) &&
            (distance < nearest ||
             (distance == nearest && entt::to_integral(mesh) < entt::to_integral(nearestMesh)))) {
            nearest = distance;
            nearestMesh = mesh;
            const auto owner = owners.find(mesh);
            target = owner == owners.end() ? entt::null : owner->second;
        }
    }
    if (target == entt::null || !registry.valid(target) ||
        !registry.all_of<Enemy, Transform, Health>(target) || registry.any_of<ToDestroy>(target) ||
        registry.get<Health>(target).current <= 0)
        return entt::null;
    return target;
}

bool BuildCursorRay(entt::registry &registry, entt::entity display, entt::entity cameraEntity,
                    GVECTORF &nearPoint, GVECTORF &farPoint)
{
    const auto &window = registry.get<GW::SYSTEM::GWindow>(display);
    bool focused = false;
    unsigned int width = 0, height = 0;
    float x = 0, y = 0;
    // Gateware reports client-relative cursor coordinates; rendering fills that client area.
    if (-window.IsFocus(focused) || !focused || -window.GetClientWidth(width) ||
        -window.GetClientHeight(height) || width == 0 || height == 0 ||
        -registry.ctx().get<UTIL::Input>().immediateInput.GetMousePosition(x, y) || x < 0 ||
        y < 0 || x >= width || y >= height)
        return false;

    GW::MATH::GMATRIXF inverseProjection;
    if (-GW::MATH::GMatrix::InverseF(registry.get<DRAW::VulkanRenderer>(display).projMatrix,
                                     inverseProjection))
        return false;
    const auto &camera = registry.get<DRAW::Camera>(cameraEntity).camMatrix;
    auto unproject = [&](float depth) {
        GVECTORF point{2 * x / width - 1, 2 * y / height - 1, depth, 1};
        GW::MATH::GMatrix::VectorXMatrixF(inverseProjection, point, point);
        point = {point.x / point.w, point.y / point.w, point.z / point.w, 1};
        GW::MATH::GMatrix::VectorXMatrixF(camera, point, point);
        return point;
    };
    nearPoint = unproject(0);
    farPoint = unproject(1);
    return true;
}

entt::entity PickEnemy(entt::registry &registry)
{
    const auto displays =
        registry.view<GW::SYSTEM::GWindow, DRAW::VulkanRenderer, DRAW::CPULevel>();
    const auto cameras = registry.view<DRAW::Camera>();
    if (displays.begin() == displays.end() || cameras.empty())
        return entt::null;
    const auto display = displays.front();
    GVECTORF nearPoint, farPoint;
    if (!BuildCursorRay(registry, display, cameras.front(), nearPoint, farPoint))
        return entt::null;
    const auto &level = registry.get<DRAW::CPULevel>(display).lvlData;
    return PickEnemyAlongRay(registry, level, nearPoint, farPoint);
}

bool IsInputDown(GW::INPUT::GInput &input, int key)
{
    float value = 0;
    input.GetState(key, value);
    return value > 0;
}

    void MovePlayer(entt::registry &registry, entt::entity player, GW::INPUT::GInput &input, float deltaTime)
    {
        const float x = float(IsInputDown(input, G_KEY_D)) - float(IsInputDown(input, G_KEY_A));
        const float z = float(IsInputDown(input, G_KEY_W)) - float(IsInputDown(input, G_KEY_S));
        const float length = std::sqrt(x * x + z * z);
        if (length > 0) {
              const float step = registry.get<Movement>(player).speed * deltaTime / length;
              auto &position = registry.get<Transform>(player).matrix.row4;
              position.x += x * step;
              position.z += z * step;
        }
    }

} // namespace

void UpdatePlayer(entt::registry &registry, float deltaTime)
{
    auto &input = registry.ctx().get<UTIL::Input>().immediateInput;
    for (auto entity : registry.view<Player, Transform, Movement, Health>()) {
        auto &player = registry.get<Player>(entity);
        const bool attackDown = IsInputDown(input, G_BUTTON_LEFT);
        const bool pressed = attackDown && !player.attackHeld;
        player.attackHeld = attackDown;
        const bool abilityDown = IsInputDown(input, G_KEY_1);
        const bool abilityPressed = abilityDown && !player.abilityHeld;
        player.abilityHeld = abilityDown;
        if (registry.get<Health>(entity).current <= 0)
            continue;
        // Pick against the camera and mesh transforms of the displayed frame, before movement.
        const auto target = pressed ? PickEnemy(registry) : entt::null;
        // Move the player using keyboard input.
        MovePlayer(registry, entity, input, deltaTime);

        // Get the player's current position.
        const auto& playerPosition =
            registry.get<Transform>(entity).matrix.row4;

        // Set the player's pickup collection radius.
        const float playerRadius = 1.0f;

        // Check for nearby pickups and collect them.
        PickUpObject(registry, entity, playerPosition, playerRadius);

        // Activate the Warrior's special ability.
        if (abilityPressed)
            TryWarriorAOE(registry, entity);
        if (target != entt::null && registry.all_of<MeleeAttack>(entity) &&
            GroundDistance(registry, entity, target) <= registry.get<MeleeAttack>(entity).range)
            TryMeleeAttack(registry, entity, target);
    }
}

void UpdateCamera(entt::registry &registry)
{
    const auto &offset = registry.ctx().get<GameplayCameraConfig>().offset;
    for (auto player : registry.view<Player, Transform>()) {
        const auto target = registry.get<Transform>(player).matrix.row4;
        const GW::MATH::GVECTORF eye{target.x + offset.x, target.y + offset.y, target.z + offset.z,
                                     1};
        GW::MATH::GMATRIXF view;
        GW::MATH::GMatrix::LookAtLHF(eye, target, {0, 1, 0, 0}, view);
        // DRAW::Camera stores a world matrix; the renderer already inverts it for the view.
        for (auto camera : registry.view<DRAW::Camera>())
            GW::MATH::GMatrix::InverseF(view, registry.get<DRAW::Camera>(camera).camMatrix);
        break;
    }
}
} // namespace GAME

