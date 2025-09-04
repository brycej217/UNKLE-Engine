#pragma once

#include "vk_mem_alloc.h"
#include "unk_device.h"
#include "unk_command_buffer.h"
#include "structs.h"
#include <vulkan/vulkan.h>
#include <unordered_map>

class UnkBlas
{
public:
	UnkDevice* device;
	UnkBuffer* blasBuffer = nullptr;
	VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
	uint64_t deviceAddress = 0;

	UnkBlas(UnkDevice* device, Mesh& mesh, VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress, VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress);

	~UnkBlas();
};