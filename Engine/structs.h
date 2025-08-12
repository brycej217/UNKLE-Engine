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
	uint32_t pad0;
	uint32_t pad1;
};

struct Mesh
{
	uint32_t indexCount = 0;
	uint32_t instanceCount = 0;
	int32_t vertexOffset = 0;

	uint32_t firstIndex = 0;
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

	void printDrawCommand()
	{
		VkDrawIndexedIndirectCommand drawCommand = getDrawCommand();

		cout << "Index count: " << drawCommand.indexCount << endl;
		cout << "Instance count: " << drawCommand.instanceCount << endl;
		cout << "First index: " << drawCommand.firstIndex << endl;
		cout << "Vertex offset: " << drawCommand.vertexOffset << endl;
		cout << "First instance: " << drawCommand.firstInstance << endl;
	};
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
	vector<VkDrawIndexedIndirectCommand> drawCommands;
	VkBuffer drawCommandBuffer;

	VkBuffer vertexSSBO;
	VkBuffer indexSSBO;

	vector<Instance> instances;
	VkBuffer instanceSSBO;
	VmaAllocationInfo instanceInfo;

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
	vec3 position;
	vec3 normal;
	vec2 texCoord;

	bool operator==(const Vertex& other) const
	{
		return position == other.position && normal == other.normal && texCoord == other.texCoord;
	}
};