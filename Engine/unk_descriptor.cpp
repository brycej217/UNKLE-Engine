#pragma once

#include "unk_descriptor.h"

UnkDescriptor::UnkDescriptor()
{
}

UnkDescriptor::~UnkDescriptor()
{
}

VkDescriptorSetLayoutBinding UnkDescriptor::getLayoutBinding()
{
	VkDescriptorSetLayoutBinding descriptorSetLayoutBinding
	{
		.binding = binding,
		.descriptorType = descriptorType,
		.descriptorCount = count,
		.stageFlags = shaderFlags
	};

	return descriptorSetLayoutBinding;
}