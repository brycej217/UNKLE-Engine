#include "unk_descriptor.h"
#include "unk_tlas.h"
#include <vulkan/vulkan.h>

class UnkAsDescriptor : public UnkDescriptor
{
public:
	UnkTlas* tlas;
	VkWriteDescriptorSetAccelerationStructureKHR info;

	UnkAsDescriptor(UnkTlas* tlas, VkDescriptorSet* descriptorSet, uint32_t binding, VkDescriptorType descriptorType, uint32_t count, VkShaderStageFlags shaderFlags, VkDescriptorBindingFlags bindingFlags);

	~UnkAsDescriptor();

	VkWriteDescriptorSet getDescriptorWrite();
};