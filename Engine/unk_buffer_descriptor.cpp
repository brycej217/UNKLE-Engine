#pragma once

#include "unk_buffer_descriptor.h"
#include "unk_buffer.h"

UnkBufferDescriptor::UnkBufferDescriptor(UnkBuffer* buffer, VkDescriptorSet* descriptorSet, uint32_t binding, VkDescriptorType descriptorType, uint32_t count, VkShaderStageFlags shaderFlags, VkDescriptorBindingFlags bindingFlags)
{
	this->buffer = buffer;
	this->descriptorSet = descriptorSet;
	this->binding = binding;
	this->descriptorType = descriptorType;
	this->count = count;
	this->shaderFlags = shaderFlags;
	this->bindingFlags = bindingFlags;
}

VkWriteDescriptorSet UnkBufferDescriptor::getDescriptorWrite()
{
	info =
	{
		.buffer = buffer->handle,
		.offset = 0,
		.range = buffer->size
	};

	VkWriteDescriptorSet descriptorWrite =
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = *descriptorSet,
		.dstBinding = binding,
		.dstArrayElement = 0,
		.descriptorCount = count,
		.descriptorType = descriptorType,
		.pBufferInfo = &info
	};

	return descriptorWrite;
}

UnkBufferDescriptor::~UnkBufferDescriptor()
{
}