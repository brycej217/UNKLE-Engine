#pragma once

#include "unk_image_descriptor.h"

UnkImageDescriptor::UnkImageDescriptor(vector<UnkImage*>& images, VkDescriptorSet* descriptorSet, uint32_t binding, VkDescriptorType descriptorType, uint32_t count, VkShaderStageFlags shaderFlags, VkDescriptorBindingFlags bindingFlags, VkImageLayout layout)
{
	this->images = images;
	this->descriptorSet = descriptorSet;
	this->binding = binding;
	this->descriptorType = descriptorType;
	this->count = count;
	this->shaderFlags = shaderFlags;
	this->bindingFlags = bindingFlags;
	this->layout = layout;
}

UnkImageDescriptor::~UnkImageDescriptor()
{

}

VkWriteDescriptorSet UnkImageDescriptor::getDescriptorWrite()
{
	infos.resize(count);
	for (int i = 0; i < count; i++)
	{
		infos[i] =
		{
			.imageView = images[i]->view,
			.imageLayout = this->layout
		};
		if (images[i]->sampler != VK_NULL_HANDLE)
		{
			infos[i].sampler = *images[i]->sampler;
		}
	}

	VkWriteDescriptorSet descriptorWrite =
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = *descriptorSet,
		.dstBinding = binding,
		.dstArrayElement = 0,
		.descriptorCount = count,
		.descriptorType = descriptorType,
		.pImageInfo = infos.data()
	};

	return descriptorWrite;
}
