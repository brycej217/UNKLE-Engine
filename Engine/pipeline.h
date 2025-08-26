#pragma once

#include <vulkan/vulkan.h>
#include "structs.h"
#include "utils.h"
#include "allocator.h"
#include <iostream>
#include <vector>
#include "vk_mem_alloc.h"

using namespace std;

class Pipeline
{
public:
	VkDevice* device;
	VkPhysicalDevice* gpu;
	Allocator* allocator;
	Swapchain* swapchain;
	PipelineFeed* pipelineFeed;

	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet descrptorSet = VK_NULL_HANDLE;

	vector<VkFramebuffer> swapchainFramebuffers;

	Pipeline() = default;

	Pipeline(VkDevice* device, VkPhysicalDevice* gpu, Allocator* allocator, Swapchain* swapchain, PipelineFeed* pipelineFeed);

	virtual ~Pipeline();

	virtual void createPipeline() = 0;

	virtual void createDescriptorSets() = 0;

	virtual void createFramebuffers() = 0;

	virtual void handleResize() = 0;

	virtual void draw(uint32_t imageIndex, vector<PerFrame>& perFrame, VkQueue* queue) = 0;

	// utility

	VkShaderModule loadShaderModule(const string& path);
};