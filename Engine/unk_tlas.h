#pragma once

#include "vk_mem_alloc.h"
#include "unk_device.h"
#include "unk_command_buffer.h"
#include "unk_blas.h"
#include "structs.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <unordered_map>


class UnkTlas
{
public:
	UnkDevice* device;
	UnkBuffer* tlasBuffer;
	VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
	uint64_t deviceAddress = 0;

	UnkTlas(UnkDevice* device, vector<Mesh>& meshes, vector<Instance>& instances, vector<MVP>& transforms, vector<UnkBlas*>& blasses);

	~UnkTlas();

	VkTransformMatrixKHR toVkTransform(const mat4& m);
};