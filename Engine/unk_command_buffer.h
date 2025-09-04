#pragma once

#include "unk_device.h"
#include "utils.h"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>

class UnkCommandBuffer
{
public:
	enum Index
	{
		GRAPHICS,
		TRANSFER,
		COMPUTE
	};

	UnkDevice* device;

	VkCommandBuffer handle{VK_NULL_HANDLE};
	VkCommandPool* commandPool{nullptr};
	uint32_t queueIndex;

	UnkCommandBuffer(UnkDevice* device, Index type, VkCommandBufferLevel level, bool begin);

	~UnkCommandBuffer();

	void beginCommand();

	void endCommand(bool submit = true);
};