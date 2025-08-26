#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>
#include "allocator.h"

void Allocator::beginTransientCommands(VkCommandPool& commandPool, VkCommandBuffer& commandBuffer, uint32_t queueIndex)
{
	// create transfer resources
	VkCommandPoolCreateInfo commandPoolInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		.queueFamilyIndex = queueIndex
	};
	VK_CHECK(vkCreateCommandPool(*device, &commandPoolInfo, nullptr, &commandPool));

	VkCommandBufferAllocateInfo commandBufferInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	VK_CHECK(vkAllocateCommandBuffers(*device, &commandBufferInfo, &commandBuffer));

	VkCommandBufferBeginInfo beginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
}

void Allocator::endTransientCommands(VkCommandPool& commandPool, VkCommandBuffer& commandBuffer, VkQueue queue)
{
	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer
	};
	VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(queue));

	// destroy transfer resources
	vkFreeCommandBuffers(*device, commandPool, 1, &commandBuffer);
	vkDestroyCommandPool(*device, commandPool, nullptr);
}

VkResult Allocator::createBuffer(VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VmaAllocationCreateFlags flags, VmaMemoryUsage vmaUsage, VkBuffer& buffer, VmaAllocationInfo* allocationInfo)
{
	VkResult result;
	VmaAllocation allocation;
	VkBufferCreateInfo uniformBufferInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = bufferSize,
		.usage = usage
	};

	VmaAllocationCreateInfo uniformBufferAllocationInfo
	{
		.flags = flags,
		.usage = vmaUsage,
		.requiredFlags = properties
	};
	result = vmaCreateBuffer(vmaAllocator, &uniformBufferInfo, &uniformBufferAllocationInfo, &buffer, &allocation, allocationInfo);

	bufferAllocationMap[buffer] = allocation;
	return result;
}

VkResult Allocator::createDeviceLocalBuffer(void* data, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VmaAllocationInfo* allocationInfo)
{
	// create staging buffer
	VkBuffer stagingBuffer;
	VmaAllocationInfo stagingBufferAllocationInfo{};

	VK_CHECK(createBuffer(bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		VMA_MEMORY_USAGE_AUTO,
		stagingBuffer,
		&stagingBufferAllocationInfo));

	if (!stagingBufferAllocationInfo.pMappedData) throw runtime_error("Could not map staging buffer");
	memcpy(stagingBufferAllocationInfo.pMappedData, data, bufferSize);

	// create device local buffer
	VK_CHECK(createBuffer(bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | properties,
		0,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		buffer,
		allocationInfo));

	// copy buffer
	copyBuffer(bufferSize, stagingBuffer, buffer);

	// destroy staging buffer
	destroyBuffer(stagingBuffer);

	return VK_SUCCESS;
}

VkResult Allocator::createDeviceLocalBuffer(void* data, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& stagingBuffer, VkBuffer& buffer, VmaAllocationInfo* allocationInfo)
{
	// create staging buffer
	VK_CHECK(createBuffer(bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		VMA_MEMORY_USAGE_AUTO,
		stagingBuffer,
		allocationInfo));

	if (!allocationInfo->pMappedData) throw runtime_error("Could not map staging buffer");
	memcpy(allocationInfo->pMappedData, data, bufferSize);

	// create device local buffer
	VK_CHECK(createBuffer(bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | properties,
		0,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		buffer,
		nullptr));

	// copy buffer
	VK_CHECK(copyBuffer(bufferSize, stagingBuffer, buffer));

	return VK_SUCCESS;
}

VkResult Allocator::copyBuffer(VkDeviceSize bufferSize, VkBuffer srcBuffer, VkBuffer dstBuffer)
{
	// copy buffers
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	beginTransientCommands(commandPool, commandBuffer, queues->transferIndex);

	VkBufferCopy copyRegion
	{
		.size = bufferSize
	};
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endTransientCommands(commandPool, commandBuffer, queues->transferQueue);

	return VK_SUCCESS;
}

VkResult Allocator::createImage(VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, uint32_t width, uint32_t height, VkImage& image, VmaAllocationInfo* allocationInfo)
{
	VkResult result;
	
	// create image texture
	VmaAllocation allocation;
	VmaAllocationCreateInfo imageAllocationInfo
	{
		.usage = memoryUsage,
	};

	VkImageCreateInfo imageCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent =
		{
			.width = width,
			.height = height,
			.depth = 1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = tiling,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	result = vmaCreateImage(vmaAllocator, &imageCreateInfo, &imageAllocationInfo, &image, &allocation, allocationInfo);
	imageAllocationMap[image] = allocation;

	return result;
}

VkResult Allocator::createTextureImage(void* data, VkDeviceSize imageSize, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage, uint32_t width, uint32_t height, VkImage& image, VmaAllocationInfo* allocationInfo)
{
	// create staging buffer
	VkBuffer stagingBuffer;
	VmaAllocationInfo stagingBufferAllocationInfo{};

	VK_CHECK(createBuffer(imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		VMA_MEMORY_USAGE_AUTO,
		stagingBuffer,
		&stagingBufferAllocationInfo));

	if (!stagingBufferAllocationInfo.pMappedData) throw runtime_error("Could not map staging buffer");

	memcpy(stagingBufferAllocationInfo.pMappedData, data, imageSize);

	// create image texture
	VK_CHECK(createImage(format, tiling, usage, memoryUsage, width, height, image, allocationInfo));

	// transition image layout to buffer writable
	transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// copy buffer to image
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	beginTransientCommands(commandPool, commandBuffer, queues->transferIndex);

	VkBufferImageCopy region
	{
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource =
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
		.imageOffset = {0, 0, 0},
		.imageExtent =
		{
			width,
			height,
			1
		}
	};

	vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	endTransientCommands(commandPool, commandBuffer, queues->transferQueue);

	// transition image layout to shader readable
	transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// destroy staging buffer
	destroyBuffer(stagingBuffer);

	return VK_SUCCESS;
}

void Allocator::transitionImageLayout(VkImage& image, VkFormat format, VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkImageMemoryBarrier barrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange =
		{
			.aspectMask = aspect,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	VkPipelineStageFlags sourceStage = VK_IMAGE_LAYOUT_UNDEFINED;
	VkPipelineStageFlags destinationStage = VK_IMAGE_LAYOUT_UNDEFINED;
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
	}

	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	beginTransientCommands(commandPool, commandBuffer, queues->graphicsIndex);

	vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	endTransientCommands(commandPool, commandBuffer, queues->graphicsQueue);
}

Allocator::Allocator(VkPhysicalDevice& gpu, VkDevice& device, VkInstance& instance, Queues& queues)
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

	VK_CHECK(vmaCreateAllocator(&allocatorInfo, &vmaAllocator));

	this->queues = &queues;
	this->device = &device;
}

/*
* DESTRUCTION
*/

void Allocator::destroyBuffer(VkBuffer buffer)
{
	auto it = bufferAllocationMap.find(buffer);
	if (it != bufferAllocationMap.end())
	{
		vmaDestroyBuffer(vmaAllocator, buffer, it->second);
		bufferAllocationMap.erase(it);
	}
}

void Allocator::destroyImage(VkImage image)
{
	auto it = imageAllocationMap.find(image);
	if (it != imageAllocationMap.end())
	{
		vmaDestroyImage(vmaAllocator, image, it->second);
		imageAllocationMap.erase(it);
	}
}

Allocator::~Allocator()
{
	for (auto& [buffer, allocation] : bufferAllocationMap)
	{
		vmaDestroyBuffer(vmaAllocator, buffer, allocation);
	}

	for (auto& [image, allocation] : imageAllocationMap)
	{
		vmaDestroyImage(vmaAllocator, image, allocation);
	}

	if (vmaAllocator != VK_NULL_HANDLE)
	{
		vmaDestroyAllocator(vmaAllocator);
	}
}