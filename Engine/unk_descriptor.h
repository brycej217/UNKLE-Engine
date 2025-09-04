#pragma once

#include <vulkan/vulkan.h>

class UnkDescriptor
{
public:
	VkDescriptorSet* descriptorSet;

	uint32_t binding;
	VkDescriptorType descriptorType;
	uint32_t count;
	VkShaderStageFlags shaderFlags;
	VkDescriptorBindingFlags bindingFlags;

	UnkDescriptor();

	~UnkDescriptor();

	VkDescriptorSetLayoutBinding getLayoutBinding();

	virtual VkWriteDescriptorSet getDescriptorWrite() = 0;
};