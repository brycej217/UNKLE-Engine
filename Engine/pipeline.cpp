#include "pipeline.h"
#include <fstream>
#include <stb_image.h>

VkShaderModule Pipeline::loadShaderModule(const string& path)
{
	ifstream file(path, ios::ate | ios::binary);

	if (!file.is_open())
	{
		throw runtime_error("failed to open file");
	}
	size_t fileSize = (size_t)file.tellg();
	vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	VkShaderModuleCreateInfo createInfo
	{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = buffer.size(),
		.pCode = reinterpret_cast<const uint32_t*>(buffer.data())
	};

	VkShaderModule shaderModule;
	VK_CHECK(vkCreateShaderModule(*device, &createInfo, nullptr, &shaderModule));
	return shaderModule;
}

Pipeline::Pipeline(VkDevice* device, VkPhysicalDevice* gpu, Allocator* allocator, Swapchain* swapchain, PipelineFeed* pipelineFeed)
{
	this->device = device;
	this->gpu = gpu;
	this->allocator = allocator;
	this->swapchain = swapchain;
	this->pipelineFeed = pipelineFeed;
}

Pipeline::~Pipeline()
{
	for (auto& framebuffer : swapchainFramebuffers)
	{
		vkDestroyFramebuffer(*device, framebuffer, nullptr);
	}

	if (descriptorSetLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(*device, descriptorSetLayout, nullptr);
	}

	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(*device, descriptorPool, nullptr);
	}

	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(*device, pipeline, nullptr);
	}

	if (pipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(*device, pipelineLayout, nullptr);
	}

	if (renderPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(*device, renderPass, nullptr);
	}
}