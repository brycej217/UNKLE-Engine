#pragma once

#include "unk_descriptor.h"
#include "unk_image.h"
#include <vulkan/vulkan.h>
#include <vector>

class UnkImageDescriptor : public UnkDescriptor
{
public:
	vector<UnkImage*> images;
	VkImageLayout layout;
	vector<VkDescriptorImageInfo> infos;

	UnkImageDescriptor(vector<UnkImage*>& images, VkDescriptorSet* descriptorSet, uint32_t binding, VkDescriptorType descriptorType, uint32_t count, VkShaderStageFlags shaderFlags, VkDescriptorBindingFlags bindingFlags, VkImageLayout layout);

	~UnkImageDescriptor();

	VkWriteDescriptorSet getDescriptorWrite();
};