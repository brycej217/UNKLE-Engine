#include "unk_image.h"

unordered_map<UnkImage*, uint32_t> UnkImage::imageMap;

UnkImage::UnkImage(UnkDevice* device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, bool view, void* data)
{
	this->device = device;
	this->size = width * height * 4;
	this->width = width;
	this->height = height;
	this->format = format;

	UnkBuffer* stagingBuffer = new UnkBuffer
	(
		device,
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
	);

	// create image texture
	VmaAllocationCreateInfo imageAllocationInfo
	{
		.usage = VMA_MEMORY_USAGE_AUTO,
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

	if (vmaCreateImage(device->allocator, &imageCreateInfo, &imageAllocationInfo, &handle, &allocation, &info) != VK_SUCCESS)
	{
		throw runtime_error("failed to create image");
	}
	memcpy(stagingBuffer->base.pMappedData, data, size);

	transitionImageLayout(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	UnkCommandBuffer* commandBuffer = new UnkCommandBuffer(device, UnkCommandBuffer::GRAPHICS, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

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

	vkCmdCopyBufferToImage(commandBuffer->handle, stagingBuffer->handle, this->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	commandBuffer->endCommand(true);

	delete commandBuffer;

	transitionImageLayout(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	UnkImage::imageMap[this] = 1;

	if (!view) return;

	// create image view
	VkImageViewCreateInfo imageViewCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = handle,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange =
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	if (vkCreateImageView(device->device, &imageViewCreateInfo, nullptr, &this->view) != VK_SUCCESS)
	{
		throw runtime_error("failed to create image view");
	}

	delete stagingBuffer;
}

UnkImage::UnkImage(UnkDevice* device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, bool view, VkImageAspectFlags aspectMask)
{
	this->device = device;
	this->size = width * height * 4;

	this->width = width;
	this->height = height;
	this->format = format;

	// create image texture
	VmaAllocationCreateInfo imageAllocationInfo
	{
		.usage = VMA_MEMORY_USAGE_AUTO,
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

	if (vmaCreateImage(device->allocator, &imageCreateInfo, &imageAllocationInfo, &handle, &allocation, &info) != VK_SUCCESS)
	{
		throw runtime_error("failed to create image");
	}

	UnkImage::imageMap[this] = 1;

	if (!view) return;

	// create image view
	VkImageViewCreateInfo imageViewCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = handle,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange =
		{
			.aspectMask = aspectMask,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	if (vkCreateImageView(device->device, &imageViewCreateInfo, nullptr, &this->view) != VK_SUCCESS)
	{
		throw runtime_error("failed to create image view");
	}
}

UnkImage::UnkImage(UnkDevice* device, VkImage& image, VkFormat format, bool view)
{
	this->device = device;
	handle = image;

	if (!view) return;

	// create image view
	VkImageViewCreateInfo imageViewCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = handle,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange =
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	if (vkCreateImageView(device->device, &imageViewCreateInfo, nullptr, &this->view) != VK_SUCCESS)
	{
		throw runtime_error("failed to create image view");
	}

	UnkImage::imageMap[this] = 1;
}

void UnkImage::transitionImageLayout(VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout, UnkCommandBuffer* commandBuffer)
{
	VkImageMemoryBarrier barrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = this->handle,
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
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = 0;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
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

	if (commandBuffer == nullptr)
	{
		UnkCommandBuffer* commandBuffer = new UnkCommandBuffer(device, UnkCommandBuffer::GRAPHICS, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		vkCmdPipelineBarrier(commandBuffer->handle, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		commandBuffer->endCommand(true);

		delete commandBuffer;
	}
	else
	{
		vkCmdPipelineBarrier(commandBuffer->handle, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}
}

UnkImage::~UnkImage()
{
	if (view != VK_NULL_HANDLE)
	{
		vkDestroyImageView(device->device, view, nullptr);
	}

	if (allocation != VK_NULL_HANDLE)
	{
		vmaDestroyImage(device->allocator, handle, allocation);
	}

	UnkImage::imageMap.erase(this);
}