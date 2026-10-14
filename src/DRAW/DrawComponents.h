#ifndef DRAW_COMPONENTS_H
#define DRAW_COMPONENTS_H


#include "./Utility/load_data_oriented.h"

namespace DRAW
{

	struct DoNotRender {};

	struct DiffuseTexture
	{
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkDescriptorPool pool = VK_NULL_HANDLE;
		VkDescriptorSet descriptor = VK_NULL_HANDLE;
	};


	struct VulkanRendererInitialization
	{
		std::string vertexShaderName;
		std::string fragmentShaderName;
		VkClearColorValue clearColor;
		VkClearDepthStencilValue depthStencil;
		float fovDegrees;
		float nearPlane;
		float farPlane;
	};

	struct VulkanRenderer
	{
		GW::GRAPHICS::GVulkanSurface vlkSurface;
		GW::CORE::GEventResponder surfaceEvents;
		VkDevice device = nullptr;
		VkPhysicalDevice physicalDevice = nullptr;
		VkRenderPass renderPass;
		VkShaderModule vertexShader = nullptr;
		VkShaderModule fragmentShader = nullptr;
		VkPipeline pipeline = nullptr;
		VkPipelineLayout pipelineLayout = nullptr;
		GW::MATH::GMATRIXF projMatrix;
		VkDescriptorSetLayout descriptorLayout = nullptr;
		VkDescriptorPool descriptorPool = nullptr;
		std::vector<VkDescriptorSet> descriptorSets;
		// Renderer owns these shared resources until shutdown; index 0 is opaque white.
		VkDescriptorSetLayout textureLayout = VK_NULL_HANDLE;
		VkSampler textureSampler = VK_NULL_HANDLE;
		std::vector<DiffuseTexture> diffuseTextures;
		std::unordered_map<std::string, unsigned int> diffuseTextureCache;
		VkClearValue clrAndDepth[2];
		bool resourcesReleased = false;
	};

	struct VulkanVertexBuffer
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
	};

	struct VulkanIndexBuffer
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
	};

	struct GeometryData
	{
		unsigned int indexStart, indexCount, vertexStart;
		unsigned int diffuseTexture = 0;
		inline bool operator < (const GeometryData a) const {
			if (indexStart != a.indexStart) return indexStart < a.indexStart;
			if (indexCount != a.indexCount) return indexCount < a.indexCount;
			if (vertexStart != a.vertexStart) return vertexStart < a.vertexStart;
			return diffuseTexture < a.diffuseTexture;
		}
	};
	
	struct GPUInstance
	{
		GW::MATH::GMATRIXF	transform;
		H2B::ATTRIBUTES		matData;
	};

	struct MeshCollection
	{
		std::vector<entt::entity> meshEntities;
		GW::MATH::GOBBF collider;
	};

	struct ModelManager
	{
		std::unordered_map<std::string, MeshCollection> models;
	};

	struct VulkanGPUInstanceBuffer
	{
		unsigned long long element_count = 1;
		std::vector<VkBuffer> buffer;
		std::vector<VkDeviceMemory> memory;
	};

	struct SceneData
	{
		GW::MATH::GVECTORF sunDirection, sunColor, sunAmbient, camPos;
		GW::MATH::GMATRIXF viewMatrix, projectionMatrix;
	};

	struct VulkanUniformBuffer
	{
		std::vector<VkBuffer> buffer;
		std::vector<VkDeviceMemory> memory;
	};


	struct Camera
	{
		GW::MATH::GMATRIXF camMatrix;
	};

	struct CPULevel
	{
		std::string levelPath;
		std::string modelFolder;
		Level_Data lvlData;
	};

	struct GPULevel
	{		
	};

}
#endif
