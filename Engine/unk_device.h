#pragma once

#include "vk_mem_alloc.h"

#include <iostream>
#include <vulkan/vulkan.h>
#include <vector>
#include <algorithm>
#include <cassert>

using namespace std;


class UnkDevice
{
public:
	VkPhysicalDevice gpu{ VK_NULL_HANDLE };
	VkDevice device{ VK_NULL_HANDLE };
	VmaAllocator allocator{ VK_NULL_HANDLE };
	VkPhysicalDeviceProperties properties{};
	VkPhysicalDeviceFeatures features{};
	VkPhysicalDeviceFeatures2 features2{};
	vector<VkQueueFamilyProperties> queueFamilyProperties{};

	VkCommandPool graphicsPool;
	VkCommandPool computePool;
	VkCommandPool transferPool;

	struct
	{
		uint32_t graphics = UINT32_MAX;
		uint32_t compute = UINT32_MAX;
		uint32_t transfer = UINT32_MAX;
	} queues;

	PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR{ nullptr };
	PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR{ nullptr };
	PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR{ nullptr };
	PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR{ nullptr };
	PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR{ nullptr };
	PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR{ nullptr };
	PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR{ nullptr };
	PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR{ nullptr };
	PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR{ nullptr };

	UnkDevice();

	UnkDevice(VkInstance instance, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures enabledFeatures, vector<const char*> enabledExtensions, void* pNext);

	~UnkDevice();

	uint32_t getQueueFamilyIndex(VkQueueFlags queueFlags) const;

	VkQueue getQueue(uint32_t index);

	void createAllocator(VkInstance instance);

	// allocated resources
	
	VkCommandPool createCommandPool(uint32_t queueIndex, VkCommandPoolCreateFlags createFlags);

	void createFunctionPointers();
};