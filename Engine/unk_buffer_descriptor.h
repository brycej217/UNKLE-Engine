#pragma once

#include "unk_descriptor.h"

#include <vulkan/vulkan.h>

struct UnkBuffer;

class UnkBufferDescriptor: public UnkDescriptor
{
public:
	UnkBuffer* buffer;
	VkDescriptorBufferInfo info;

	UnkBufferDescriptor(UnkBuffer* buffer, VkDescriptorSet* descriptorSet, uint32_t binding, VkDescriptorType descriptorType, uint32_t count, VkShaderStageFlags shaderFlags, VkDescriptorBindingFlags bindingFlags);

	~UnkBufferDescriptor();

	VkWriteDescriptorSet getDescriptorWrite();
};