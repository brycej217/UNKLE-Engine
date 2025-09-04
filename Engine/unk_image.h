#pragma once

#include "unk_device.h"
#include "unk_buffer.h"
#include "unk_command_buffer.h"
#include "unk_buffer_descriptor.h"

#include "vk_mem_alloc.h"
#include <vulkan/vulkan.h>

#include <unordered_map>


class UnkImage
{
public:
	UnkDevice* device;

	VkDeviceSize size;
	VkImage handle = VK_NULL_HANDLE;
	
	VkImageView view = VK_NULL_HANDLE;
	VkSampler* sampler = VK_NULL_HANDLE;

	uint32_t width;
	uint32_t height;
	VkFormat format{};

	VmaAllocation allocation;
	VmaAllocationInfo info;

	static unordered_map<UnkImage*, uint32_t> imageMap;

	UnkImage(UnkDevice* device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, bool view, void* data);

	UnkImage(UnkDevice* device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, bool view, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

	UnkImage(UnkDevice* device, VkImage& image, VkFormat format, bool view);

	~UnkImage();

	void transitionImageLayout(VkImageAspectFlags  aspect, VkImageLayout oldLayout, VkImageLayout newLayout, UnkCommandBuffer* commandBuffer = nullptr);
};