#include "unk_as_descriptor.h"

UnkAsDescriptor::UnkAsDescriptor(UnkTlas* tlas, VkDescriptorSet* descriptorSet, uint32_t binding, VkDescriptorType descriptorType, uint32_t count, VkShaderStageFlags shaderFlags, VkDescriptorBindingFlags bindingFlags)
{
	this->tlas = tlas;
	this->descriptorSet = descriptorSet;
	this->binding = binding;
	this->descriptorType = descriptorType;
	this->count = count;
	this->shaderFlags = shaderFlags;
	this->bindingFlags = bindingFlags;
}

UnkAsDescriptor::~UnkAsDescriptor()
{

}

VkWriteDescriptorSet UnkAsDescriptor::getDescriptorWrite()
{
	info =
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
		.accelerationStructureCount = 1,
		.pAccelerationStructures = &tlas->handle
	};

	VkWriteDescriptorSet descriptorWrite =
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = &info,
		.dstSet = *descriptorSet,
		.dstBinding = 3,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
	};

	return descriptorWrite;
}