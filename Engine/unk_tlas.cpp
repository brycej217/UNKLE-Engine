#include "unk_tlas.h"

UnkTlas::UnkTlas(UnkDevice* device, vector<Mesh>& meshes, vector<Instance>& instances, vector<MVP>& transforms, vector<UnkBlas*>& blasses)
{
	this->device = device;

	vector<VkAccelerationStructureInstanceKHR> asInstances;

	for (int i = 0; i < meshes.size(); i++)
	{
		for (int j = 0; j < meshes[i].instanceCount; j++)
		{
			uint32_t instIndex = meshes[i].firstInstance + j;
			Instance instRec = instances[instIndex];
			mat4 matrix = transforms[instRec.transformIndex].model;

			VkAccelerationStructureInstanceKHR asInstance
			{
				.transform = toVkTransform(matrix),
				.instanceCustomIndex = instIndex,
				.mask = 0xFF,
				.instanceShaderBindingTableRecordOffset = 0,
				.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
				.accelerationStructureReference = blasses[i]->deviceAddress
			};
			asInstances.push_back(asInstance);
		}
	}

	const uint32_t instanceCount = asInstances.size();

	UnkBuffer* instanceBuffer = new UnkBuffer
	(
		device,
		sizeof(VkAccelerationStructureInstanceKHR) * instanceCount,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0,
		asInstances.data()
	);

	// ge instance buffer device address
	VkBufferDeviceAddressInfoKHR instanceBufferDeviceAI
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = instanceBuffer->handle
	};
	VkDeviceAddress instanceBufferDeviceAddress = device->vkGetBufferDeviceAddressKHR(device->device, &instanceBufferDeviceAI);

	VkAccelerationStructureGeometryKHR accelerationStructureGeometry
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
		.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
		.geometry =
		{
			.instances =
			{
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
				.arrayOfPointers = VK_FALSE,
				.data =
				{
					.deviceAddress = instanceBufferDeviceAddress
				}
			}
		},
		.flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
	};

	VkAccelerationStructureBuildGeometryInfoKHR sizeInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		.geometryCount = 1,
		.pGeometries = &accelerationStructureGeometry
	};
	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
	};
	device->vkGetAccelerationStructureBuildSizesKHR
	(
		device->device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&sizeInfo,
		&instanceCount,
		&accelerationStructureBuildSizesInfo
	);

	// create top level acceleration structure buffer
	tlasBuffer = new UnkBuffer
	(
		device,
		accelerationStructureBuildSizesInfo.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	);

	VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
		.buffer = tlasBuffer->handle,
		.size = accelerationStructureBuildSizesInfo.accelerationStructureSize,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
	};
	device->vkCreateAccelerationStructureKHR(device->device, &accelerationStructureCreateInfo, nullptr, &this->handle);

	// create scratch buffer
	UnkBuffer* scratchBuffer = new UnkBuffer
	(
		device,
		accelerationStructureBuildSizesInfo.buildScratchSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	);

	// get scratch buffer device address
	VkBufferDeviceAddressInfoKHR bufferDeviceAI
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = scratchBuffer->handle
	};
	VkDeviceAddress scratchAddress = device->vkGetBufferDeviceAddressKHR(device->device, &bufferDeviceAI);

	VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo
	{
		.primitiveCount = instanceCount,
		.primitiveOffset = 0,
		.firstVertex = 0,
		.transformOffset = 0
	};
	vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

	// create top level acceleration structure
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		.dstAccelerationStructure = this->handle,
		.geometryCount = 1,
		.pGeometries = &accelerationStructureGeometry,
		.scratchData =
		{
			.deviceAddress = scratchAddress
		}
	};

	UnkCommandBuffer* commandBuffer = new UnkCommandBuffer(device, UnkCommandBuffer::GRAPHICS, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	device->vkCmdBuildAccelerationStructuresKHR(commandBuffer->handle, 1, &buildInfo, accelerationBuildStructureRangeInfos.data());

	commandBuffer->endCommand(true);

	VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
		.accelerationStructure = this->handle
	};
	this->deviceAddress = device->vkGetAccelerationStructureDeviceAddressKHR(device->device, &accelerationDeviceAddressInfo);

	delete instanceBuffer;
	delete scratchBuffer;
	delete commandBuffer;
}

VkTransformMatrixKHR UnkTlas::toVkTransform(const mat4& m)
{
	VkTransformMatrixKHR out{};

	out.matrix[0][0] = m[0][0];
	out.matrix[0][1] = m[1][0];
	out.matrix[0][2] = m[2][0];
	out.matrix[0][3] = m[3][0];

	out.matrix[1][0] = m[0][1];
	out.matrix[1][1] = m[1][1];
	out.matrix[1][2] = m[2][1];
	out.matrix[1][3] = m[3][1];

	out.matrix[2][0] = m[0][2];
	out.matrix[2][1] = m[1][2];
	out.matrix[2][2] = m[2][2];
	out.matrix[2][3] = m[3][2];
	return out;
}


UnkTlas::~UnkTlas()
{
	if (handle != VK_NULL_HANDLE)
	{
		device->vkDestroyAccelerationStructureKHR(device->device, handle, nullptr);
		handle = VK_NULL_HANDLE;
	}
	
	if (tlasBuffer != nullptr) delete tlasBuffer;
}
