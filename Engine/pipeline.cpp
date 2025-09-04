#include "pipeline.h"
#include <fstream>
#include <stb_image.h>

Pipeline::Pipeline()
{

}

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
	vkCreateShaderModule(device->device, &createInfo, nullptr, &shaderModule);
	return shaderModule;
}

Pipeline::~Pipeline()
{
	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(device->device, descriptorPool, nullptr);
	}

	if (descriptorSetLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(device->device, descriptorSetLayout, nullptr);
	}

	for (auto& descriptor : descriptors)
	{
		delete descriptor;
	}

	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device->device, pipeline, nullptr);
	}

	if (pipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(device->device, pipelineLayout, nullptr);
	}
}