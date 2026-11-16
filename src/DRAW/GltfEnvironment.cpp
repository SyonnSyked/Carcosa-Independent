#include "Utility/load_data_oriented.h"
// Gateware's Windows headers expose macros that conflict with fastgltf's math API.
#undef min
#undef max
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <cfloat>
#include <numeric>

void Level_Data::AppendGltfEnvironment(const std::filesystem::path& path,
                                     const GW::MATH::GVECTORF& scale,
                                     const GW::MATH::GVECTORF& translation)
{
    auto fail = [&](const std::string& reason) {
        throw std::runtime_error("glTF environment " + path.string() + ": " + reason);
    };
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None)
        fail(std::string(fastgltf::getErrorMessage(data.error())));
    fastgltf::Parser parser;
    auto result = parser.loadGltf(data.get(), path.parent_path(), fastgltf::Options::LoadExternalBuffers);
    if (result.error() != fastgltf::Error::None)
        fail(std::string(fastgltf::getErrorMessage(result.error())));
    auto& asset = result.get();
    if (fastgltf::validate(asset) != fastgltf::Error::None || asset.scenes.empty())
        fail("invalid asset or no scene");
    if (!asset.skins.empty() || !asset.animations.empty())
        fail("only static scenery is supported by this importer");
    if (!std::isfinite(scale.x) || !std::isfinite(scale.y) || !std::isfinite(scale.z)
        || scale.x == 0 || scale.y == 0 || scale.z == 0
        || !std::isfinite(translation.x) || !std::isfinite(translation.y) || !std::isfinite(translation.z))
        fail("invalid environment scale/translation");

    fastgltf::math::fmat4x4 placement;
    placement[0][0] = scale.x;
    placement[1][1] = scale.y;
    placement[2][2] = scale.z;
    const auto scene = asset.defaultScene.value_or(0);
    const auto firstModel = levelModels.size();
    fastgltf::iterateSceneNodes(asset, scene, placement,
        [&](fastgltf::Node& node, const fastgltf::math::fmat4x4& world) {
        if (!node.meshIndex) return;
        const auto linear = fastgltf::math::fmat3x3(world);
        const float determinant = fastgltf::math::determinant(linear);
        if (!std::isfinite(determinant) || std::abs(determinant) < 1e-10f)
            fail("singular node transform");
        const auto normalMatrix = fastgltf::math::transpose(fastgltf::math::inverse(linear));
        for (const auto& primitive : asset.meshes[*node.meshIndex].primitives) {
            if (primitive.type != fastgltf::PrimitiveType::Triangles || !primitive.targets.empty())
                fail("only triangle meshes without morph targets are supported");
            const auto position = primitive.findAttribute("POSITION");
            const auto normal = primitive.findAttribute("NORMAL");
            if (position == primitive.attributes.end() || normal == primitive.attributes.end())
                fail("meshes must include positions and normals; enable Normals in Blender export");
            const auto count = asset.accessors[position->accessorIndex].count;
            if (!count || asset.accessors[normal->accessorIndex].count != count)
                fail("invalid position/normal counts");
            std::vector<H2B::VERTEX> vertices(count);
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset,
                asset.accessors[position->accessorIndex], [&](auto p, std::size_t i) {
                const auto v = world * fastgltf::math::fvec4(p[0], p[1], p[2], 1.f);
                // glTF is right handed; the game's camera/world use left handed +Z.
                vertices[i].pos = {v[0] + translation.x, v[1] + translation.y, -v[2] + translation.z};
            });
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset,
                asset.accessors[normal->accessorIndex], [&](auto n, std::size_t i) {
                const auto v = fastgltf::math::normalize(normalMatrix * n);
                vertices[i].nrm = {v[0], v[1], -v[2]};
            });
            std::vector<unsigned> indices;
            if (primitive.indicesAccessor) {
                fastgltf::iterateAccessor<unsigned>(asset, asset.accessors[*primitive.indicesAccessor],
                    [&](unsigned index) { indices.push_back(index); });
            } else {
                indices.resize(count);
                std::iota(indices.begin(), indices.end(), 0u);
            }
            if (indices.empty() || indices.size() % 3 != 0) fail("invalid triangle index count");
            for (auto index : indices) if (index >= count) fail("index outside vertex buffer");
            if (determinant > 0)
                for (std::size_t i = 0; i < indices.size(); i += 3) std::swap(indices[i + 1], indices[i + 2]);

            H2B::MATERIAL material{};
            material.attrib.Kd = {1, 1, 1};
            material.attrib.Ka = {1, 1, 1};
            material.attrib.d = 1;
            bool doubleSided = false;
            if (primitive.materialIndex) {
                const auto& source = asset.materials[*primitive.materialIndex];
                if (source.pbrData.baseColorTexture || source.alphaMode != fastgltf::AlphaMode::Opaque)
                    fail("textured/transparent glTF materials are not supported yet");
                const auto& color = source.pbrData.baseColorFactor;
                material.attrib.Kd = {color[0], color[1], color[2]};
                material.attrib.Ke = {source.emissiveFactor[0], source.emissiveFactor[1], source.emissiveFactor[2]};
                doubleSided = source.doubleSided;
            }
            if (doubleSided) {
                const auto frontCount = indices.size();
                for (std::size_t i = 0; i < frontCount; i += 3)
                    indices.insert(indices.end(), {indices[i], indices[i + 2], indices[i + 1]});
            }
            const auto name = "gltf:" + path.filename().string() + ":" + std::to_string(levelModels.size());
            const auto stableName = level_strings.insert(name).first->c_str();
            LEVEL_MODEL model{};
            model.filename = stableName;
            model.vertexStart = static_cast<unsigned>(levelVertices.size());
            model.vertexCount = static_cast<unsigned>(vertices.size());
            model.indexStart = static_cast<unsigned>(levelIndices.size());
            model.indexCount = static_cast<unsigned>(indices.size());
            model.materialStart = static_cast<unsigned>(levelMaterials.size());
            model.materialCount = 1;
            model.meshStart = static_cast<unsigned>(levelMeshes.size());
            model.batchStart = static_cast<unsigned>(levelBatches.size());
            model.meshCount = 1;
            model.colliderIndex = static_cast<unsigned>(levelColliders.size());
            GW::MATH::GVECTORF low{FLT_MAX, FLT_MAX, FLT_MAX, 1}, high{-FLT_MAX, -FLT_MAX, -FLT_MAX, 1};
            for (const auto& vertex : vertices) {
                low.x = std::min(low.x, vertex.pos.x); high.x = std::max(high.x, vertex.pos.x);
                low.y = std::min(low.y, vertex.pos.y); high.y = std::max(high.y, vertex.pos.y);
                low.z = std::min(low.z, vertex.pos.z); high.z = std::max(high.z, vertex.pos.z);
            }
            GW::MATH::GOBBF bounds{GW::MATH::GIdentityVectorF, GW::MATH::GIdentityVectorF,
                                    GW::MATH::GIdentityQuaternionF};
            bounds.center.x = (low.x + high.x)/2;
            bounds.center.y = (low.y + high.y)/2;
            bounds.center.z = (low.z + high.z)/2;
            bounds.extent.x = (high.x-low.x)/2;
            bounds.extent.y = (high.y-low.y)/2;
            bounds.extent.z = (high.z-low.z)/2;
            levelColliders.push_back(bounds);
            // Scenery bounds are retained, but a floor must not become a horizontal movement obstacle.
            const auto modelIndex = static_cast<unsigned>(levelModels.size());
            const auto transformIndex = static_cast<unsigned>(levelTransforms.size());
            levelVertices.insert(levelVertices.end(), vertices.begin(), vertices.end());
            levelIndices.insert(levelIndices.end(), indices.begin(), indices.end());
            levelMaterials.push_back(material);
            levelBatches.push_back({model.indexCount, 0});
            levelMeshes.push_back({stableName, {model.indexCount, 0}, 0});
            levelModels.push_back(model);
            levelTransforms.push_back(GW::MATH::GIdentityMatrixF);
            levelInstances.push_back({modelIndex, transformIndex, 1, 0});
            blenderObjects.push_back({stableName, modelIndex, transformIndex});
        }
    });
    if (levelModels.size() == firstModel) fail("scene contains no triangle geometry");
    std::cout << "glTF environment loaded: " << path.string() << " ("
              << levelModels.size() - firstModel << " primitives)\n";
}
