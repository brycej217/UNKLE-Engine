#pragma once

#include "unk_device.h"

UnkDevice::UnkDevice()
{
}

UnkDevice::UnkDevice(VkInstance instance, VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures enabledFeatures, vector<const char*> enabledExtensions, void* pNext)
{
	this->gpu = physicalDevice;

	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	vkGetPhysicalDeviceFeatures(physicalDevice, &features);

	// queue family properties
	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	assert(queueFamilyCount > 0);
	queueFamilyProperties.resize(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

	// query queue indices and create creation infos
	const float queuePriority = 1.0f;
	vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	queues.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
	VkDeviceQueueCreateInfo gQueueInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = queues.graphics,
		.queueCount = 1,
		.pQueuePriorities = &queuePriority,
	};
	queueCreateInfos.push_back(gQueueInfo);

	queues.compute = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
	VkDeviceQueueCreateInfo cQueueInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = queues.compute,
		.queueCount = 1,
		.pQueuePriorities = &queuePriority,
	};
	queueCreateInfos.push_back(cQueueInfo);

	queues.transfer = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
	VkDeviceQueueCreateInfo tQueueInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = queues.transfer,
		.queueCount = 1,
		.pQueuePriorities = &queuePriority,
	};
	queueCreateInfos.push_back(tQueueInfo);

	VkDeviceCreateInfo deviceCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = nullptr,
		.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
		.pQueueCreateInfos = queueCreateInfos.data(),
		.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size()),
		.ppEnabledExtensionNames = enabledExtensions.data(),
		.pEnabledFeatures = nullptr
	};

	if (pNext)
	{
		deviceCreateInfo.pEnabledFeatures;
		deviceCreateInfo.pNext = pNext;
	}
	else
	{
		deviceCreateInfo.pEnabledFeatures = &enabledFeatures;
	}

	if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS)
	{
		throw runtime_error("error creating logical device");
	}

	createAllocator(instance);

	// default command pool
	graphicsPool = createCommandPool(queues.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	computePool = createCommandPool(queues.compute, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	transferPool = createCommandPool(queues.transfer, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

	// create function pointers
	createFunctionPointers();
}

uint32_t UnkDevice::getQueueFamilyIndex(VkQueueFlags queueFlags) const
{
	if ((queueFlags & VK_QUEUE_COMPUTE_BIT) == queueFlags)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
		{
			if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
			{
				return i;
			}
		}
	}

	if ((queueFlags & VK_QUEUE_TRANSFER_BIT) == queueFlags)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
		{
			if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
			{
				return i;
			}
		}
	}

	for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
	{
		if ((queueFamilyProperties[i].queueFlags & queueFlags) == queueFlags)
		{
			return i;
		}
	}

	throw std::runtime_error("could not find matching queue family");
}

VkQueue UnkDevice::getQueue(uint32_t index)
{
	VkQueue queue;
	vkGetDeviceQueue(device, index, 0, &queue);
	return queue;
}

void UnkDevice::createAllocator(VkInstance instance)
{
	VmaVulkanFunctions vmaVulkanFunc
	{
		.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
		.vkGetDeviceProcAddr = vkGetDeviceProcAddr
	};

	VmaAllocatorCreateInfo allocatorInfo
	{
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = gpu,
		.device = device,
		.pVulkanFunctions = &vmaVulkanFunc,
		.instance = instance
	};

	if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS)
	{
		throw runtime_error("failed to create vma allocator");
	}
}

VkCommandPool UnkDevice::createCommandPool(uint32_t queueIndex, VkCommandPoolCreateFlags createFlags)
{
	VkCommandPool commandPool;
	VkCommandPoolCreateInfo commandPoolInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = createFlags,
		.queueFamilyIndex = queueIndex
	};
	vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool);
	return commandPool;
}

void UnkDevice::createFunctionPointers()
{
	vkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR");
	vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
	vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR");
	vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");
	vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
	vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR");
	vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");
	vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
	vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
}

UnkDevice::~UnkDevice()
{
	vkDeviceWaitIdle(device);

	if (graphicsPool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(device, graphicsPool, nullptr);
	}
	if (computePool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(device, computePool, nullptr);
	}
	if (transferPool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(device, transferPool, nullptr);
	}
	if (allocator)
	{
		vmaDestroyAllocator(allocator);
	}
	if (device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(device, nullptr);
	}
}