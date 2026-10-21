#include "precompiled.h"
#include "src/CCL.h"
#include "src/DRAW/DrawComponents.h"
#include "src/GAME/GameManager.h"
#include "src/GAME/GameplaySystems.h"
#include "src/UTIL/Utilities.h"
#include <stdexcept>

using namespace GAME;
namespace {
void Check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
Transform At(float x, float z)
{
    auto matrix = GW::MATH::GIdentityMatrixF;
    matrix.row4 = {x, 0, z, 1};
    return {matrix};
}
entt::entity EnemyAt(entt::registry &r, float x, float z, float hp = 100)
{
    const auto e = r.create();
    r.emplace<Enemy>(e, Enemy{});
    r.emplace<Transform>(e, At(x, z));
    r.emplace<Health>(e, Health{hp, hp});
    return e;
}
void ClearAxes(entt::registry &r)
{
    std::vector<entt::entity> axes;
    for (auto e : r.view<ThrownAxe>()) axes.push_back(e);
    for (auto e : axes) r.destroy(e);
}
}

int main()
{
    try {
        entt::registry r;
        CCL::InitializeComponentLogic(r);
        r.ctx().emplace<DamageFeedbackConfig>(DamageFeedbackConfig{false, .12f, 1, 1, 1});
        // Use the real generated model and the production cloning/cleanup callbacks.
        H2B::Parser model;
        Check(model.Parse("assets/models/ThrowingAxe.h2b"), "axe H2B parses");
        Check(model.meshCount == 3 && model.indexCount == 708, "downloaded axe has three material groups");
        auto &source = r.ctx().emplace<DRAW::ModelManager>().models["ThrowingAxe"];
        for (const auto &mesh : model.meshes) {
            const auto e = r.create();
            source.meshEntities.push_back(e);
            r.emplace<DRAW::GeometryData>(e, DRAW::GeometryData{
                mesh.drawInfo.indexOffset, mesh.drawInfo.indexCount, 0});
            r.emplace<DRAW::GPUInstance>(e, DRAW::GPUInstance{
                At(0, 0).matrix, model.materials[mesh.materialIndex].attrib});
            r.emplace<DRAW::DoNotRender>(e);
        }
        const auto templates = source.meshEntities;
        const auto p = r.create();
        r.emplace<Player>(p);
        r.emplace<Transform>(p, At(0, 0));
        r.emplace<Health>(p, Health{100, 100});
        r.emplace<MeleeAttack>(p, MeleeAttack{25, 5, .5f});
        r.emplace<AxeThrow>(p);
        r.emplace<WarriorAOE>(p);
        TryThrowAxe(r, p, {0, 0, 0, 1});
        Check(r.view<ThrownAxe>().empty(), "zero aim creates no axe");
        Check(r.get<AxeThrow>(p).remaining == 0, "zero aim costs no cooldown");
        TryThrowAxe(r, p, {10, 0, 0, 1});
        Check(r.view<ThrownAxe>().size() == 1, "throw creates one axe");
        auto axe = r.view<ThrownAxe>().front();
        Check(std::abs(r.get<ThrownAxe>(axe).damage - 15) < .001f, "15 damage versus 25 melee");
        Check(r.get<DRAW::MeshCollection>(axe).meshEntities.size() == 3, "visible axe meshes cloned");
        TryThrowAxe(r, p, {10, 0, 0, 1});
        Check(r.view<ThrownAxe>().size() == 1, "cooldown blocks spam between throws");
        Check(r.get<MeleeAttack>(p).remaining == 0 && r.get<WarriorAOE>(p).remaining == 0,
              "throw does not consume melee or AOE cooldown");
        UpdateCombatTimers(r, .35f);
        TryThrowAxe(r, p, {10, 0, 0, 1});
        Check(r.view<ThrownAxe>().size() == 2, "repeat after short cooldown");
        ClearAxes(r);
        r.get<AxeThrow>(p).remaining = 0;
        const auto nearEnemy = EnemyAt(r, 2, 0), farEnemy = EnemyAt(r, 4, 0);
        TryThrowAxe(r, p, {10, 0, 0, 1});
        axe = r.view<ThrownAxe>().front();
        const auto owned = r.get<DRAW::MeshCollection>(axe).meshEntities;
        UpdateThrownAxes(r, 1); // Crosses both enemies in one long frame.
        Check(r.get<Health>(nearEnemy).current == 85 && r.get<Health>(farEnemy).current == 100,
              "swept hit chooses first enemy, does not pierce");
        Check(r.get<Health>(p).current == 100, "axe cannot hurt player");
        Check(r.all_of<ToDestroy>(axe), "impact removes axe");
        UpdateThrownAxes(r, 1);
        Check(r.get<Health>(nearEnemy).current == 85, "impact damages once");
        r.destroy(axe);
        for (auto e : owned) Check(!r.valid(e), "axe-owned meshes cleaned up");
        for (auto e : templates) Check(r.valid(e), "shared templates survive cleanup");
        r.destroy(nearEnemy); r.destroy(farEnemy);
        auto miss = EnemyAt(r, 2, 2);
        auto distant = EnemyAt(r, 9, 0);
        r.get<AxeThrow>(p).remaining = 0;
        TryThrowAxe(r, p, {20, 0, 0, 1});
        axe = r.view<ThrownAxe>().front();
        UpdateThrownAxes(r, .1f);
        Check(!r.all_of<ToDestroy>(axe), "axe remains visible during flight");
        Check(std::abs(r.get<Transform>(axe).matrix.row1.x - 1) > .01f, "axe spins");
        UpdateThrownAxes(r, 5);
        Check(r.all_of<ToDestroy>(axe), "miss expires at maximum range");
        Check(std::abs(r.get<Transform>(axe).matrix.row4.x - 8) < .001f, "long frame capped to eight units");
        Check(r.get<Health>(miss).current == 100 && r.get<Health>(distant).current == 100,
              "off-path and out-of-range enemies unharmed");
        ClearAxes(r); r.destroy(miss); r.destroy(distant);
        auto weak = EnemyAt(r, 2, 0, 10);
        r.get<AxeThrow>(p).remaining = 0;
        TryThrowAxe(r, p, {10, 0, 0, 1});
        UpdateThrownAxes(r, 1);
        Check(r.get<Health>(weak).current == 0 && r.all_of<ToDestroy>(weak), "lethal hit uses death handling");
        ClearAxes(r); r.destroy(weak);
        r.get<Health>(p).current = 0;
        r.get<AxeThrow>(p).remaining = 0;
        TryThrowAxe(r, p, {10, 0, 0, 1});
        Check(r.view<ThrownAxe>().empty(), "dead player cannot throw");
        r.destroy(p);

        // Load the real level to verify the hidden axe template and restart integration.
        r.ctx().emplace<UTIL::Config>();
        const auto levelEntity = r.create();
        auto &data = r.emplace<DRAW::CPULevel>(levelEntity,
            DRAW::CPULevel{"assets/levels/Level1.json", "assets/models", {}}).lvlData;
        auto &models = r.ctx().get<DRAW::ModelManager>().models;
        models.clear();
        for (const auto &object : data.blenderObjects) {
            const auto &m = data.levelModels[object.modelIndex];
            if (!m.isDynamic) continue;
            auto &collection = models[object.blendername];
            collection.collider = data.levelColliders[m.colliderIndex];
            for (unsigned i = 0; i < m.meshCount; ++i) {
                const auto &mesh = data.levelMeshes[m.meshStart + i];
                auto e = r.create();
                collection.meshEntities.push_back(e);
                r.emplace<DRAW::GeometryData>(e, DRAW::GeometryData{
                    m.indexStart + mesh.drawInfo.indexOffset, mesh.drawInfo.indexCount, m.vertexStart});
                r.emplace<DRAW::GPUInstance>(e, DRAW::GPUInstance{
                    data.levelTransforms[object.transformIndex],
                    data.levelMaterials[m.materialStart + mesh.materialIndex].attrib});
                r.emplace<DRAW::DoNotRender>(e);
            }
        }
        Check(models.contains("ThrowingAxe") && models.at("ThrowingAxe").meshEntities.size() == 3,
              "level loads dynamic axe template");
        InitializeGameplay(r);
        const auto player = r.view<Player>().front();
        Check(r.all_of<AxeThrow>(player), "warrior receives secondary attack");
        const auto position = r.get<Transform>(player).matrix.row4;
        TryThrowAxe(r, player, {position.x + 10, position.y, position.z, 1});
        auto oldAxe = r.view<ThrownAxe>().front();
        const auto oldMeshes = r.get<DRAW::MeshCollection>(oldAxe).meshEntities;
        RestartGame(r);
        Check(!r.valid(oldAxe) && r.view<ThrownAxe>().empty(), "restart removes airborne axes");
        for (auto e : oldMeshes) Check(!r.valid(e), "restart removes axe meshes");
        Check(r.view<Player>().size() == 1 && r.view<AxeThrow>().size() == 1, "restart creates fresh warrior");
        std::cout << "PASS: axe model, damage, cooldown, repeat, swept first hit, spin, range, miss, death, cleanup, level, restart\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
