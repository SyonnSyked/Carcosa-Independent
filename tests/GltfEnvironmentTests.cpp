#include "src/DRAW/Utility/load_data_oriented.h"
#include <stdexcept>

void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main()
{
    try {
        GW::SYSTEM::GLog log;
        log.Create("build/dev/GltfEnvironmentTestLog.txt");
        Level_Data level;
        Check(level.LoadLevel("assets/levels/Level_1.json", "assets/models", log), "manifest failed");
        Check(level.levelModels.size() == 10, "all ten gameplay templates must load");
        for (const auto& model : level.levelModels)
            Check(model.isDynamic, "old scenery must not appear in the new level");
        const auto vertexStart = level.levelVertices.size();
        level.AppendGltfEnvironment("assets/levels/Level_1.glb", {30, 1, 30, 0}, {0, -0.125164f, 0, 0});
        Check(level.levelVertices.size() == vertexStart + 2152, "actual floor vertex count");
        const auto& floor = level.levelModels.back();
        Check(!floor.isDynamic && !floor.isCollidable, "floor must render without blocking actors");
        Check(floor.indexCount == 7056, "double-sided floor triangles");
        const auto& bounds = level.levelColliders.at(floor.colliderIndex);
        Check(bounds.extent.x > 29 && bounds.extent.z > 29, "arena spans approximately 60 units");
        Check(std::abs(bounds.center.y + bounds.extent.y) < 0.001f, "floor top is at gameplay ground zero");
        for (unsigned i = 0; i < floor.indexCount; ++i)
            Check(level.levelIndices.at(floor.indexStart + i) < floor.vertexCount, "indices stay inside floor mesh");
        for (std::size_t i = vertexStart; i < level.levelVertices.size(); ++i) {
            const auto& v = level.levelVertices[i];
            Check(std::isfinite(v.pos.x) && std::isfinite(v.pos.y) && std::isfinite(v.pos.z), "finite positions");
            const auto length = v.nrm.x*v.nrm.x + v.nrm.y*v.nrm.y + v.nrm.z*v.nrm.z;
            Check(std::abs(length - 1) < 0.001f, "normalized transformed normals");
        }
        level.UnloadLevel();
        Check(level.levelColliders.empty(), "unload clears GLB and H2B bounds");
        bool rejected = false;
        try { level.AppendGltfEnvironment("assets/levels/missing.glb", {1,1,1,0}, {0,0,0,0}); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "missing environment must fail explicitly");
        std::cout << "GLB floor and legacy gameplay template checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
