#include "precompiled.h"
#include "src/DRAW/VulkanTextures.h"
#include <fstream>

void Require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

int main()
{
    try {
        for (auto model : {"Warrior", "VampireCleric", "Room1", "Room2", "Enemy", "PurpleSpellEffect"}) {
            H2B::Parser parser;
            Require(parser.Parse((std::string("assets/models/") + model + ".h2b").c_str()), "Cannot parse model");
            unsigned int texturedMeshes = 0;
            for (const auto &mesh : parser.meshes) {
                const auto &material = parser.materials.at(mesh.materialIndex);
                const auto path = DRAW::FindDiffusePng(material);
                std::cout << model << " / " << (mesh.name ? mesh.name : "") << " / "
                          << (material.name ? material.name : "") << " -> "
                          << (path.empty() ? "original Kd" : path.filename().string()) << '\n';
                if (path.empty()) continue;
                ++texturedMeshes;
                Require(!material.map_Kd || !*material.map_Kd, "First targets should use empty-map material-name lookup");
                auto png = DRAW::DecodePng(path);
                Require(png.width && png.height && png.rgba.size() == std::uint64_t(png.width) * png.height * 4,
                        "Incorrect PNG RGBA layout");
                bool hasUv = false;
                for (unsigned int i = mesh.drawInfo.indexOffset;
                     i < mesh.drawInfo.indexOffset + mesh.drawInfo.indexCount; ++i) {
                    const auto uv = parser.vertices.at(parser.indices.at(i)).uvw;
                    Require(std::isfinite(uv.x) && std::isfinite(uv.y), "Nonfinite UV");
                    hasUv |= uv.x != 0 || uv.y != 0;
                }
                Require(hasUv, "Missing authored UV coordinates");
                DRAW::GeometryData geometry{mesh.drawInfo.indexOffset, mesh.drawInfo.indexCount, 0, texturedMeshes};
                DRAW::GeometryData otherTexture{mesh.drawInfo.indexOffset, mesh.drawInfo.indexCount, 0, texturedMeshes + 1};
                Require(geometry < otherTexture && !(otherTexture < geometry), "Different textures must have distinct draw groups");
            }
            const bool target = std::string(model) == "Warrior" || std::string(model) == "VampireCleric";
            Require(target ? texturedMeshes == 2 : texturedMeshes == 0, "Unexpected material association");
        }
        H2B::MATERIAL material{};
        material.name = "Warrior_Texture";
        auto inferred = DRAW::FindDiffusePng(material);
        material.map_Kd = "Warrior_Texture.png";
        Require(DRAW::FindDiffusePng(material) == inferred, "Explicit path lookup mismatch");
        material.name = "unrelated";
        Require(DRAW::FindDiffusePng(material) == inferred, "Explicit map must take priority");
        for (auto bad : {"missing.png", "../models/Warrior.h2b", "C:/outside.png", "image.jpg"}) {
            material.map_Kd = bad;
            bool rejected = false;
            try { DRAW::FindDiffusePng(material); }
            catch (const std::exception &error) {
                rejected = std::string(error.what()).find(bad) != std::string::npos;
            }
            Require(rejected, "Explicit missing/unsafe/unsupported map did not report its path");
        }
        material.map_Kd = nullptr;
        material.name = "../Warrior_Texture";
        Require(DRAW::FindDiffusePng(material).empty(), "Ambiguous material name inferred a texture");
        material.name = nullptr;
        Require(DRAW::FindDiffusePng(material).empty(), "Empty material inferred a texture");
        const std::filesystem::path invalid = "build/invalid-texture-test.png";
        { std::ofstream output(invalid); output << "not a PNG"; }
        bool rejected = false;
        try { DRAW::DecodePng(invalid); }
        catch (const std::exception &error) {
            rejected = std::string(error.what()).find(invalid.string()) != std::string::npos;
        }
        std::filesystem::remove(invalid);
        Require(rejected, "Corrupt PNG did not report its precise path");
        std::cout << "PASS: real H2B material associations, RGBA PNG decode, UV audit, unchanged untextured materials, explicit lookup/errors, draw group texture identity\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
