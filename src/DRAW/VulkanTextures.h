#pragma once
#include "DrawComponents.h"

namespace DRAW {
struct DecodedPng {
    unsigned int width, height;
    std::vector<unsigned char> rgba;
};

// Empty result means an untextured material. Explicit references fail with the path.
std::filesystem::path FindDiffusePng(const H2B::MATERIAL &material,
                                   const std::filesystem::path &root = "assets/textures");
DecodedPng DecodePng(const std::filesystem::path &path);
void InitializeDiffuseTextures(VulkanRenderer &renderer);
unsigned int LoadDiffuseTexture(VulkanRenderer &renderer, const H2B::MATERIAL &material);
void DestroyDiffuseTextures(VulkanRenderer &renderer);
} // namespace DRAW
