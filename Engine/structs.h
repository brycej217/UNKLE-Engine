#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <vulkan/vulkan.h>
#include <iostream>
#include <set>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "vk_mem_alloc.h"

using namespace glm;
using namespace std;

struct PerFrame
{
	VkImage swapchainImage = VK_NULL_HANDLE;
	VkFence queueSubmitFence = VK_NULL_HANDLE;
	VkSemaphore swapchainAcquireSemaphore = VK_NULL_HANDLE;
	VkSemaphore swapchainReleaseSemaphore = VK_NULL_HANDLE;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
};


struct Queues
{
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;
	VkQueue transferQueue = VK_NULL_HANDLE;

	uint32_t graphicsIndex = UINT32_MAX;
	uint32_t presentIndex = UINT32_MAX;
	uint32_t transferIndex = UINT32_MAX;

	set<uint32_t> uniqueIndices = {};
};

struct SwapchainDimensions
{
	uint32_t width = 0;
	uint32_t height = 0;

	VkFormat format = VK_FORMAT_UNDEFINED;
};

struct Swapchain
{
	SwapchainDimensions dimensions;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	vector<VkImageView> imageViews;

	void cleanup(VkDevice* device)
	{
		for (auto& swapchainImageView : imageViews)
		{
			vkDestroyImageView(*device, swapchainImageView, nullptr);
		}

		if (swapchain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(*device, swapchain, nullptr);
		}
	}
};

struct Instance
{
	uint32_t transformIndex;
	uint32_t textureIndex;
	uint32_t baseIndex;
	uint32_t baseVertex;
};

struct Mesh
{
	int32_t vertexOffset = 0; // first vertex corrseponding to this mesh
	uint32_t vertexCount = 0;

	uint32_t indexCount = 0;
	uint32_t firstIndex = 0;

	uint32_t instanceCount = 0;
	uint32_t firstInstance = -1;

	VkDrawIndexedIndirectCommand getDrawCommand()
	{
		VkDrawIndexedIndirectCommand drawCommand
		{
			.indexCount = this->indexCount,
			.instanceCount = this->instanceCount,
			.firstIndex = this->firstIndex,
			.vertexOffset = this->vertexOffset,
			.firstInstance = this->firstInstance
		};

		return drawCommand;
	}
};

struct Light
{
	vec3 position;
	vec3 direction;
	vec3 color;
};

struct MVP
{
	mat4 model;
	mat4 view;
	mat4 proj;
};

struct PipelineFeed
{
	vector<Mesh> meshes;

	vector<VkDrawIndexedIndirectCommand> drawCommands;
	VkBuffer drawCommandBuffer;

	VkBuffer vertexSSBO;
	VkDeviceSize vertexBufferSize;

	VkBuffer indexSSBO;
	VkDeviceSize indexBufferSize;

	vector<Instance> instances;
	VkBuffer instanceSSBO;
	VmaAllocationInfo instanceInfo;

	VkBuffer cameraBuffer;
	VmaAllocationInfo cameraInfo;

	vector<MVP> transforms;
	VkBuffer transformSSBO;
	VkBuffer transformStagingBuffer;
	VmaAllocationInfo transformInfo;

	vector<VkImage> textureImages;
	vector<VkImageView> textureImageViews;
	VkSampler textureImageSampler = VK_NULL_HANDLE;

	vector<Light> lights;
	VkBuffer lightSSBO;
	VkBuffer lightStagingBuffer;
	VmaAllocationInfo lightInfo;

	void cleanup(VkDevice* device)
	{
		for (auto& textureImageView : textureImageViews)
		{
			vkDestroyImageView(*device, textureImageView, nullptr);
		}

		if (textureImageSampler != VK_NULL_HANDLE)
		{
			vkDestroySampler(*device, textureImageSampler, nullptr);
		}
	}
};

struct Transform
{
	vec3 position = vec3(0.0f);
	quat rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);

	mat4 getWorldMatrix() const
	{
		return translate(mat4(1.0f), position) * mat4_cast(rotation);
	}

	vec3 getForward() const { return rotation * vec3(0.0f, 0.0f, -1.0f); }
	vec3 getUp() const { return rotation * vec3(0.0f, 1.0f, 0.0f); }
	vec3 getRight() const { return rotation * vec3(1.0f, 0.0f, 0.0f); }
};

struct CameraGPU
{
	mat4 viewInv;
	mat4 projInv;
};

struct Camera
{
	Transform transform;

	mat4 getViewMatrix() const
	{
		return inverse(transform.getWorldMatrix());
	}

	vec3 getForward() const { return transform.getForward(); }
	vec3 getUp() const { return transform.getUp(); }
	vec3 getRight() const { return transform.getRight(); }
};

struct Vertex
{
	vec3 position; float _pad0;
	vec3 normal; float _pad1;
	vec2 texCoord; vec2 _pad2;

	bool operator==(const Vertex& other) const
	{
		return position == other.position && normal == other.normal && texCoord == other.texCoord;
	}
};