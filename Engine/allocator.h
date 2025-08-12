#pragma once

#include "vk_mem_alloc.h"
#include "utils.h"
#include "structs.h"

#include <vector>
#include <unordered_map>

using namespace std;

class Allocator
{
	VkDevice* device;
	VmaAllocator vmaAllocator = VK_NULL_HANDLE;
	Queues* queues;

	unordered_map<VkBuffer, VmaAllocation> bufferAllocationMap;
	unordered_map<VkImage, VmaAllocation> imageAllocationMap;

public:
	Allocator() = default;

	Allocator(VkPhysicalDevice& gpu, VkDevice& device, VkInstance& instance, Queues& queues);

	void beginTransientCommands(VkCommandPool& commandPool, VkCommandBuffer& commandBuffer, uint32_t queueIndex);

	void endTransientCommands(VkCommandPool& commandPool, VkCommandBuffer& commandBuffer, VkQueue queue);

	VkResult createBuffer(VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VmaAllocationCreateFlags flags, VmaMemoryUsage vmaUsage, VkBuffer& buffer, VmaAllocationInfo* allocationInfo);

	VkResult createDeviceLocalBuffer(void* data, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VmaAllocationInfo* alloactionInfo);

	VkResult createDeviceLocalBuffer(void* data, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& stagingBuffer, VkBuffer& buffer, VmaAllocationInfo* allocationInfo);

	VkResult copyBuffer(VkDeviceSize bufferSize, VkBuffer srcBuffer, VkBuffer dstBuffer);

	VkResult createImage(VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, uint32_t width, uint32_t height, VkImage& image,VmaAllocationInfo* allocationInfo);

	VkResult createTextureImage(void* data, VkDeviceSize imageSize, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, uint32_t width, uint32_t height, VkImage& image, VmaAllocationInfo* allocationInfo);
	
	void transitionImageLayout(VkImage& image, VkFormat format, VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout);

	// destruction

	~Allocator();

	void destroyBuffer(VkBuffer buffer);

	void destroyImage(VkImage image);

private:
};

