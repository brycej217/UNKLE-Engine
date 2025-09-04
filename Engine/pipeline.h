#pragma once

#include "unk_device.h"
#include "unk_swapchain.h"
#include "unk_buffer.h"
#include "unk_buffer_descriptor.h"
#include "unk_image.h"
#include "unk_image_descriptor.h"
#include "unk_command_buffer.h"

#include <vulkan/vulkan.h>
#include "structs.h"
#include "utils.h"
#include <iostream>
#include <vector>
#include "vk_mem_alloc.h"

using namespace std;

class Pipeline
{
public:
	UnkDevice* device;
	UnkSwapchain* swapchain;
	DeviceResources* resources;

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	vector<UnkDescriptor*> descriptors;

	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

	Pipeline();

	virtual ~Pipeline();

	virtual void createPipeline() = 0;

	virtual void createDescriptorSets() = 0;

	virtual void handleResize() = 0;

	virtual void draw(uint32_t imageIndex) = 0;

	// utility

	VkShaderModule loadShaderModule(const string& path);
};