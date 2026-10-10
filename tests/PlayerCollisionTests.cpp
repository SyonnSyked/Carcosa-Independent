// Standalone deterministic check; compile with the game's include paths and precompiled.h.
// Includes the implementation so the geometry checks use the exact production math.
#include "../precompiled.h"
#include "../src/GAME/Collision.cpp"
#include <cassert>
#include <iostream>

namespace {
using namespace GAME;
entt::entity MakePlayer(entt::registry &r, float x, float z)
{
    auto e = r.create();
    r.emplace<Player>(e);
    Transform t{GW::MATH::GIdentityMatrixF};
    t.matrix.row4 = {x, 7, z, 1};
    r.emplace<Transform>(e, t);
    return e;
}
entt::entity Wall(entt::registry &r, float halfX, float halfZ,
                  GW::MATH::GMATRIXF matrix = GW::MATH::GIdentityMatrixF)
{
    auto e = r.create();
    r.emplace<Collidable>(e);
    r.emplace<Obstacle>(e);
    r.emplace<Transform>(e, Transform{matrix});
    GW::MATH::GOBBF box{};
    box.center = {0, 0, 0, 1};
    box.extent = {halfX, 1, halfZ, 0};
    box.rotation = GW::MATH::GIdentityQuaternionF;
    r.emplace<DRAW::MeshCollection>(e, DRAW::MeshCollection{{}, box});
    return e;
}
void ClearOfWalls(entt::registry &r, entt::entity p)
{
    const auto pos = r.get<Transform>(p).matrix.row4;
    for (auto e : r.view<Collidable, Obstacle, Transform, DRAW::MeshCollection>(entt::exclude<ToDestroy>)) {
        const auto hull = WorldFootprint(r.get<DRAW::MeshCollection>(e), r.get<Transform>(e));
        const auto push = PushOut(hull, {pos.x, pos.z}, r.ctx().get<CollisionConfig>().playerRadius);
        assert(Dot2(push, push) == 0);
    }
    assert(pos.y == 7 && pos.w == 1);
}
}
int main()
{
    using namespace GAME;
    {
        entt::registry r;
        r.ctx().emplace<CollisionConfig>(CollisionConfig{0.5f});
        auto p = MakePlayer(r, 0, 0);
        // Input still supplies speed * deltaTime / length; collision must preserve
        // the resulting displacement in free space, independent of frame subdivision.
        const float diagonal = 5.0f / std::sqrt(2.0f);
        MovePlayerWithCollision(r, p, diagonal, diagonal);
        const auto single = r.get<Transform>(p).matrix.row4;
        assert(std::abs(std::sqrt(single.x * single.x + single.z * single.z) - 5) < 0.001f);
        r.get<Transform>(p).matrix.row4 = {0, 7, 0, 1};
        for (int i = 0; i < 100; ++i)
            MovePlayerWithCollision(r, p, diagonal / 100, diagonal / 100);
        assert(std::abs(r.get<Transform>(p).matrix.row4.x - single.x) < 0.001f);
        assert(std::abs(r.get<Transform>(p).matrix.row4.z - single.z) < 0.001f);
        ClearOfWalls(r, p);
    }
    {
        entt::registry r;
        r.ctx().emplace<CollisionConfig>(CollisionConfig{0.5f});
        auto p = MakePlayer(r, -4, 0);
        auto w = Wall(r, 0.01f, 10);
        MovePlayerWithCollision(r, p, 100, 3); // Long displacement through a thin wall.
        auto pos = r.get<Transform>(p).matrix.row4;
        assert(pos.x < -0.51f && std::abs(pos.z - 3) < 0.001f);
        ClearOfWalls(r, p);
        for (int i = 0; i < 100; ++i)
            MovePlayerWithCollision(r, p, 0.1f, 0);
        ClearOfWalls(r, p);
        r.remove<Collidable>(w);
        MovePlayerWithCollision(r, p, 2, 0);
        assert(r.get<Transform>(p).matrix.row4.x > 0);
        r.emplace<Collidable>(w);
        r.remove<Obstacle>(w);
        MovePlayerWithCollision(r, p, -4, 0);
        assert(r.get<Transform>(p).matrix.row4.x < -2);
        r.emplace<Obstacle>(w);
        r.emplace<ToDestroy>(w);
        MovePlayerWithCollision(r, p, 4, 0);
        assert(r.get<Transform>(p).matrix.row4.x > 0);
    }
    {
        entt::registry r;
        r.ctx().emplace<CollisionConfig>(CollisionConfig{0.5f});
        GW::MATH::GMATRIXF m;
        GW::MATH::GMatrix::RotationYawPitchRollF(0.785398163f, 0, 0, m);
        m.row1.x *= 2; m.row1.z *= 2; // Nonuniform local scale.
        m.row4 = {5, 0, -3, 1};
        auto w = Wall(r, 0.1f, 20, m);
        auto p = MakePlayer(r, 5 - 3 * m.row1.x, -3 - 3 * m.row1.z);
        const auto before = r.get<Transform>(p).matrix;
        MovePlayerWithCollision(r, p, 6 * m.row1.x + 2 * m.row3.x,
                                      6 * m.row1.z + 2 * m.row3.z);
        ClearOfWalls(r, p);
        auto pos = r.get<Transform>(p).matrix.row4;
        const double tangentTravel = (pos.x - before.row4.x) * m.row3.x +
                                     (pos.z - before.row4.z) * m.row3.z;
        assert(std::abs(tangentTravel - 2) < 0.001);
        // Local off-center bounds must receive the world transform too.
        r.get<DRAW::MeshCollection>(w).collider.center.x = 2;
        r.get<Transform>(p).matrix.row4 = {5, 7, -3, 1};
        MovePlayerWithCollision(r, p, 100 * m.row1.x, 100 * m.row1.z);
        ClearOfWalls(r, p);
        // Model's Y/rotation/scale must survive position correction.
        for (int i = 0; i < 12; ++i)
            assert(r.get<Transform>(p).matrix.data[i] == before.data[i]);
    }
    {
        entt::registry r;
        r.ctx().emplace<CollisionConfig>(CollisionConfig{0.5f});
        auto w = Wall(r, 1, 1);
        auto p = MakePlayer(r, 0, 0); // Initially inside: resolve to a valid position.
        MovePlayerWithCollision(r, p, 0, 0);
        ClearOfWalls(r, p);
        r.get<Transform>(p).matrix.row4 = {-1.5f, 7, 0, 1}; // Exact initial contact.
        MovePlayerWithCollision(r, p, 100, 0);
        assert(r.get<Transform>(p).matrix.row4.x < -1.5f);
        ClearOfWalls(r, p);
        // Rounded corner permits space a square expansion would incorrectly block.
        r.get<Transform>(p).matrix.row4 = {1.4f, 7, 1.4f, 1};
        MovePlayerWithCollision(r, p, -0.04f, 0);
        assert(std::abs(r.get<Transform>(p).matrix.row4.x - 1.36f) < 0.001f);
        ClearOfWalls(r, p);
        // A tilted box needs all eight projected corners, not just local X/Z extents.
        GW::MATH::GMatrix::RotationYawPitchRollF(0.3f, 0.7f, 0.2f,
                                               r.get<Transform>(w).matrix);
        r.get<Transform>(p).matrix.row4 = {-5, 7, 0, 1};
        MovePlayerWithCollision(r, p, 100, 0);
        ClearOfWalls(r, p);
    }
    {
        entt::registry r;
        r.ctx().emplace<CollisionConfig>(CollisionConfig{0.25f});
        Wall(r, 0.01f, 100);
        auto m = GW::MATH::GIdentityMatrixF;
        m.row4.z = 2;
        Wall(r, 100, 0.01f, m);
        auto p = MakePlayer(r, -4, -4);
        MovePlayerWithCollision(r, p, 100, 100);
        ClearOfWalls(r, p);
        assert(r.get<Transform>(p).matrix.row4.x < -0.25f);
        assert(r.get<Transform>(p).matrix.row4.z < 1.75f);
    }
    // Deterministic coverage over many obstacle rotations, signed scales and long sweeps.
    for (int i = 0; i < 100; ++i) {
        entt::registry r;
        const float radius = 0.15f + i * 0.005f;
        r.ctx().emplace<CollisionConfig>(CollisionConfig{radius});
        GW::MATH::GMATRIXF m;
        GW::MATH::GMatrix::RotationYawPitchRollF(i * 0.073f, i * 0.013f, i * 0.021f, m);
        const float sx = (i % 2 ? -1 : 1) * (0.2f + i * 0.03f);
        m.row1.x *= sx; m.row1.y *= sx; m.row1.z *= sx;
        m.row4 = {i * 0.2f, 2, i * -0.3f, 1};
        Wall(r, 0.02f, 2, m);
        const float angle = i * 0.19f;
        const float dx = std::cos(angle), dz = std::sin(angle);
        auto p = MakePlayer(r, m.row4.x - 10 * dx, m.row4.z - 10 * dz);
        MovePlayerWithCollision(r, p, 1000 * dx, 1000 * dz);
        ClearOfWalls(r, p);
    }
    std::cout << "Player collision checks passed.\n";
}
