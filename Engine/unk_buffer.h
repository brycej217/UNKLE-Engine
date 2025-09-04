#pragma once

#include "vk_mem_alloc.h"
#include "unk_device.h"
#include "unk_command_buffer.h"
#include <vulkan/vulkan.h>
#include <unordered_map>

using namespace std;

class UnkBuffer
{
public:
	UnkDevice* device;

	VkDeviceSize size;
	VkBuffer handle{ VK_NULL_HANDLE };
	VmaAllocation allocation{VK_NULL_HANDLE};
	VmaAllocationInfo base{};

	VkBuffer stagingHandle{ VK_NULL_HANDLE };
	VmaAllocation stagingAllocation{VK_NULL_HANDLE};
	VmaAllocationInfo stagingBase{};

	static unordered_map<UnkBuffer*, uint32_t> bufferMap;
	
	UnkBuffer(UnkDevice* device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VmaAllocationCreateFlags flags);

	UnkBuffer(UnkDevice* device, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VmaAllocationCreateFlags flags, void* data, bool staging = false);

	void copy(UnkBuffer* other);

	void stage();

	~UnkBuffer();
};