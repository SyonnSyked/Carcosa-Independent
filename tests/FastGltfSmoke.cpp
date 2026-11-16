#include <fastgltf/core.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include <iostream>
#include <string_view>

int main()
{
    // No renderer, Vulkan device, or external asset is needed to verify the library.
    constexpr std::string_view scene = R"({
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "CarcosaSmoke"}]
    })";
    auto data = fastgltf::GltfDataBuffer::FromBytes(
        reinterpret_cast<const std::byte*>(scene.data()), scene.size());
    if (data.error() != fastgltf::Error::None) {
        std::cerr << "Could not create glTF buffer\n";
        return 1;
    }
    fastgltf::Parser parser;
    auto asset = parser.loadGltf(data.get(), std::filesystem::path{});
    if (asset.error() != fastgltf::Error::None) {
        std::cerr << "Could not parse glTF: " << fastgltf::getErrorMessage(asset.error()) << '\n';
        return 1;
    }
    if (asset->nodes.size() != 1 || asset->nodes[0].name != "CarcosaSmoke"
        || asset->scenes.size() != 1 || asset->scenes[0].nodeIndices.size() != 1
        || asset->scenes[0].nodeIndices[0] != 0) {
        std::cerr << "Parsed glTF scene does not match input\n";
        return 1;
    }
    std::cout << "fastgltf linked and parsed a glTF scene successfully\n";
}
