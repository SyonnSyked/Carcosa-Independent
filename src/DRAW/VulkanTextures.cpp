#include "VulkanTextures.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace DRAW {
namespace {
void Check(VkResult result, const char *operation)
{
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
}
std::string PathKey(const std::filesystem::path &path)
{
    auto key = path.generic_string();
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
    return key;
}
unsigned int MemoryType(VulkanRenderer &r, unsigned int bits, VkMemoryPropertyFlags flags)
{
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(r.physicalDevice, &properties);
    for (unsigned int i = 0; i < properties.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (properties.memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    throw std::runtime_error("No Vulkan memory type for diffuse texture");
}
void DestroyTexture(VkDevice device, const DiffuseTexture &texture)
{
    vkDestroyDescriptorPool(device, texture.pool, nullptr);
    vkDestroyImageView(device, texture.view, nullptr);
    vkDestroyImage(device, texture.image, nullptr);
    vkFreeMemory(device, texture.memory, nullptr);
}

// One synchronous startup upload. No work is submitted on the frame command buffers.
DiffuseTexture Upload(VulkanRenderer &r, const DecodedPng &pixels)
{
    DiffuseTexture texture;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    bool submitted = false;
    auto cleanup = [&] {
        vkDestroyFence(r.device, fence, nullptr);
        vkDestroyCommandPool(r.device, commandPool, nullptr);
        vkDestroyBuffer(r.device, staging, nullptr);
        vkFreeMemory(r.device, stagingMemory, nullptr);
    };
    try {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(r.physicalDevice, &properties);
        if (!pixels.width || !pixels.height ||
            pixels.width > properties.limits.maxImageDimension2D ||
            pixels.height > properties.limits.maxImageDimension2D ||
            pixels.rgba.size() != std::uint64_t(pixels.width) * pixels.height * 4)
            throw std::runtime_error("Diffuse PNG dimensions/pixel layout exceed device limits");
        VkFormatProperties format;
        vkGetPhysicalDeviceFormatProperties(r.physicalDevice, VK_FORMAT_R8G8B8A8_SRGB, &format);
        const auto required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                              VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                              VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
        if ((format.optimalTilingFeatures & required) != required)
            throw std::runtime_error("Device does not support filtered RGBA8 sRGB diffuse images");

        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = pixels.rgba.size();
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        Check(vkCreateBuffer(r.device, &bufferInfo, nullptr, &staging), "Create texture staging buffer");
        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(r.device, staging, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = MemoryType(r, requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        Check(vkAllocateMemory(r.device, &allocation, nullptr, &stagingMemory), "Allocate texture staging memory");
        Check(vkBindBufferMemory(r.device, staging, stagingMemory, 0), "Bind texture staging memory");
        void *mapped = nullptr;
        Check(vkMapMemory(r.device, stagingMemory, 0, pixels.rgba.size(), 0, &mapped), "Map texture pixels");
        std::memcpy(mapped, pixels.rgba.data(), pixels.rgba.size());
        vkUnmapMemory(r.device, stagingMemory);

        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        imageInfo.extent = {pixels.width, pixels.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Check(vkCreateImage(r.device, &imageInfo, nullptr, &texture.image), "Create diffuse image");
        vkGetImageMemoryRequirements(r.device, texture.image, &requirements);
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = MemoryType(r, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Check(vkAllocateMemory(r.device, &allocation, nullptr, &texture.memory), "Allocate diffuse image memory");
        Check(vkBindImageMemory(r.device, texture.image, texture.memory, 0), "Bind diffuse image memory");

        void *queueHandle = nullptr;
        unsigned int graphicsFamily = 0, presentFamily = 0;
        if (-r.vlkSurface.GetGraphicsQueue(&queueHandle) ||
            -r.vlkSurface.GetQueueFamilyIndices(graphicsFamily, presentFamily))
            throw std::runtime_error("Graphics queue unavailable for texture upload");
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex = graphicsFamily;
        Check(vkCreateCommandPool(r.device, &poolInfo, nullptr, &commandPool), "Create texture upload pool");
        VkCommandBufferAllocateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        commandInfo.commandPool = commandPool;
        commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandInfo.commandBufferCount = 1;
        VkCommandBuffer command;
        Check(vkAllocateCommandBuffers(r.device, &commandInfo, &command), "Allocate texture upload command");
        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        Check(vkBeginCommandBuffer(command, &beginInfo), "Begin texture upload");
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture.image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = imageInfo.extent;
        vkCmdCopyBufferToImage(command, staging, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        Check(vkEndCommandBuffer(command), "End texture upload");
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        Check(vkCreateFence(r.device, &fenceInfo, nullptr, &fence), "Create texture upload fence");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        Check(vkQueueSubmit(static_cast<VkQueue>(queueHandle), 1, &submit, fence), "Submit texture upload");
        submitted = true;
        Check(vkWaitForFences(r.device, 1, &fence, VK_TRUE, UINT64_MAX), "Wait for texture upload");
        submitted = false;

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = texture.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = imageInfo.format;
        viewInfo.subresourceRange = barrier.subresourceRange;
        Check(vkCreateImageView(r.device, &viewInfo, nullptr, &texture.view), "Create diffuse image view");
        VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1}, {VK_DESCRIPTOR_TYPE_SAMPLER, 1}};
        VkDescriptorPoolCreateInfo descriptorPoolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        descriptorPoolInfo.maxSets = 1;
        descriptorPoolInfo.poolSizeCount = 2;
        descriptorPoolInfo.pPoolSizes = sizes;
        Check(vkCreateDescriptorPool(r.device, &descriptorPoolInfo, nullptr, &texture.pool), "Create diffuse descriptor pool");
        VkDescriptorSetAllocateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        setInfo.descriptorPool = texture.pool;
        setInfo.descriptorSetCount = 1;
        setInfo.pSetLayouts = &r.textureLayout;
        Check(vkAllocateDescriptorSets(r.device, &setInfo, &texture.descriptor), "Allocate diffuse descriptor");
        VkDescriptorImageInfo imageDescriptor{VK_NULL_HANDLE, texture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo samplerDescriptor{r.textureSampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
        VkWriteDescriptorSet writes[2]{};
        for (auto &write : writes) {
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = texture.descriptor;
            write.descriptorCount = 1;
        }
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[0].pImageInfo = &imageDescriptor;
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        writes[1].pImageInfo = &samplerDescriptor;
        vkUpdateDescriptorSets(r.device, 2, writes, 0, nullptr);
        cleanup();
        return texture;
    } catch (...) {
        if (submitted) vkDeviceWaitIdle(r.device);
        cleanup();
        DestroyTexture(r.device, texture);
        throw;
    }
}
} // namespace

std::filesystem::path FindDiffusePng(const H2B::MATERIAL &material, const std::filesystem::path &root)
{
    const bool explicitMap = material.map_Kd && *material.map_Kd;
    if (!explicitMap && (!material.name || !*material.name)) return {};
    const std::filesystem::path name = explicitMap ? material.map_Kd : std::string(material.name) + ".png";
    auto fail = [&](const std::string &reason) -> std::filesystem::path {
        if (explicitMap) throw std::runtime_error("Diffuse PNG '" + (root / name).string() + "': " + reason);
        return {};
    };
    // Names are exact conventions, never guesses. Do not allow references outside texture root.
    if (name.has_root_path() || (!explicitMap && name.has_parent_path()))
        return fail("expected a relative PNG path inside the texture folder");
    auto base = std::filesystem::weakly_canonical(root);
    auto path = std::filesystem::weakly_canonical(base / name);
    auto relative = path.lexically_relative(base);
    if (relative.empty() || *relative.begin() == "..") return fail("path escapes the texture folder");
    if (PathKey(path.extension()) != ".png") return fail("only PNG diffuse images are supported");
    if (!std::filesystem::is_regular_file(path)) return fail("file does not exist");
    return path;
}

DecodedPng DecodePng(const std::filesystem::path &path)
{
    using Microsoft::WRL::ComPtr;
    auto check = [&](HRESULT result, const char *operation) {
        if (FAILED(result)) {
            std::ostringstream error;
            error << "Diffuse PNG '" << path.string() << "': " << operation
                  << " failed (HRESULT 0x" << std::hex << static_cast<unsigned long>(result) << ")";
            throw std::runtime_error(error.str());
        }
    };
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) check(initialized, "COM initialization");
    struct ComScope {
        bool uninitialize;
        ~ComScope() { if (uninitialize) CoUninitialize(); }
    } scope{SUCCEEDED(initialized)};
    ComPtr<IWICImagingFactory> factory;
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(factory.GetAddressOf())), "create Windows PNG decoder");
    ComPtr<IWICBitmapDecoder> decoder;
    check(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
          WICDecodeMetadataCacheOnLoad, &decoder), "open/decode file");
    GUID container;
    check(decoder->GetContainerFormat(&container), "read container format");
    if (container != GUID_ContainerFormatPng)
        throw std::runtime_error("Diffuse PNG '" + path.string() + "': file is not a PNG");
    ComPtr<IWICBitmapFrameDecode> frame;
    check(decoder->GetFrame(0, &frame), "read PNG frame");
    DecodedPng pixels{};
    check(frame->GetSize(&pixels.width, &pixels.height), "read dimensions");
    const std::uint64_t bytes = std::uint64_t(pixels.width) * pixels.height * 4;
    if (!pixels.width || !pixels.height || bytes > (std::numeric_limits<UINT>::max)())
        throw std::runtime_error("Diffuse PNG '" + path.string() + "': invalid or oversized RGBA dimensions");
    ComPtr<IWICFormatConverter> converter;
    check(factory->CreateFormatConverter(&converter), "create RGBA converter");
    check(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
          nullptr, 0, WICBitmapPaletteTypeCustom), "convert to RGBA8");
    pixels.rgba.resize(static_cast<std::size_t>(bytes));
    // Keep decoder row order and authored UVs; no speculative V flip or alpha blending.
    check(converter->CopyPixels(nullptr, pixels.width * 4, static_cast<UINT>(bytes), pixels.rgba.data()), "read RGBA pixels");
    return pixels;
}

void InitializeDiffuseTextures(VulkanRenderer &r)
{
    try {
        VkDescriptorSetLayoutBinding bindings[2]{};
        bindings[0] = {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        bindings[1] = {1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings = bindings;
        Check(vkCreateDescriptorSetLayout(r.device, &layoutInfo, nullptr, &r.textureLayout), "Create diffuse descriptor layout");
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.maxLod = 0; // Single mip, no anisotropy feature requirement.
        Check(vkCreateSampler(r.device, &samplerInfo, nullptr, &r.textureSampler), "Create diffuse sampler");
        auto white = Upload(r, DecodedPng{1, 1, {255, 255, 255, 255}});
        try { r.diffuseTextures.push_back(white); }
        catch (...) { DestroyTexture(r.device, white); throw; }
    } catch (...) {
        DestroyDiffuseTextures(r);
        throw;
    }
}

unsigned int LoadDiffuseTexture(VulkanRenderer &r, const H2B::MATERIAL &material)
{
    const auto path = FindDiffusePng(material);
    if (path.empty()) return 0;
    const auto key = PathKey(path);
    if (const auto found = r.diffuseTextureCache.find(key); found != r.diffuseTextureCache.end()) return found->second;
    try {
        const auto pixels = DecodePng(path);
        auto texture = Upload(r, pixels);
        auto index = static_cast<unsigned int>(r.diffuseTextures.size());
        try { r.diffuseTextures.push_back(texture); }
        catch (...) { DestroyTexture(r.device, texture); throw; }
        try { r.diffuseTextureCache.emplace(key, index); }
        catch (...) { r.diffuseTextures.pop_back(); DestroyTexture(r.device, texture); throw; }
        std::cout << "Diffuse texture [" << index << "]: " << path.string() << " ("
                  << pixels.width << "x" << pixels.height << ", RGBA8 sRGB)\n";
        return index;
    } catch (const std::exception &error) {
        throw std::runtime_error("Diffuse PNG '" + path.string() + "': " + error.what());
    }
}

void DestroyDiffuseTextures(VulkanRenderer &r)
{
    // Caller waits for the device before destroying descriptors and sampled images.
    for (const auto &texture : r.diffuseTextures) DestroyTexture(r.device, texture);
    r.diffuseTextures.clear();
    r.diffuseTextureCache.clear();
    vkDestroySampler(r.device, r.textureSampler, nullptr);
    vkDestroyDescriptorSetLayout(r.device, r.textureLayout, nullptr);
    r.textureSampler = VK_NULL_HANDLE;
    r.textureLayout = VK_NULL_HANDLE;
}
} // namespace DRAW
