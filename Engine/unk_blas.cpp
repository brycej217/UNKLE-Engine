#pragma once

#include "unk_blas.h"

UnkBlas::UnkBlas(UnkDevice* device, Mesh& mesh, VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress, VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress)
{
	this->device = device;

	const uint32_t primitiveCount = mesh.indexCount / 3;

	// describe bottom level acceleration structure geometry
	VkAccelerationStructureGeometryKHR accelerationStructureGeometry
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
		.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
		.geometry =
		{
			.triangles =
			{
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
				.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
				.vertexData = vertexBufferDeviceAddress.deviceAddress + offsetof(Vertex, position) + VkDeviceSize(mesh.vertexOffset) * sizeof(Vertex),
				.vertexStride = sizeof(Vertex),
				.maxVertex = mesh.vertexCount - 1,
				.indexType = VK_INDEX_TYPE_UINT32,
				.indexData = indexBufferDeviceAddress.deviceAddress + VkDeviceSize(mesh.firstIndex) * sizeof(uint32_t),
				.transformData =
				{
					.deviceAddress = 0,
				},
			}
		},
		.flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
	};

	VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo
	{
		.primitiveCount = primitiveCount,
		.primitiveOffset = 0,
		.firstVertex = 0,
		.transformOffset = 0
	};

	/// BUILD ACCELERATION STRUCTURE ///

	VkAccelerationStructureBuildGeometryInfoKHR sizeInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		.geometryCount = 1,
		.pGeometries = &accelerationStructureGeometry
	};

	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	device->vkGetAccelerationStructureBuildSizesKHR(
		device->device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&sizeInfo,
		&primitiveCount,
		&accelerationStructureBuildSizesInfo
	);

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

	// create bottom level acceleration structure buffer
	blasBuffer = new UnkBuffer
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
		.buffer = blasBuffer->handle,
		.size = accelerationStructureBuildSizesInfo.accelerationStructureSize,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
	};
	device->vkCreateAccelerationStructureKHR(device->device, &accelerationStructureCreateInfo, nullptr, &this->handle);

	// create bottom level acceleration structure
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
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

	vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

	UnkCommandBuffer* commandBuffer = new UnkCommandBuffer(device, UnkCommandBuffer::GRAPHICS, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	device->vkCmdBuildAccelerationStructuresKHR(commandBuffer->handle, 1, &buildInfo, accelerationBuildStructureRangeInfos.data());

	commandBuffer->endCommand(true);

	VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
		.accelerationStructure = this->handle
	};
	this->deviceAddress = device->vkGetAccelerationStructureDeviceAddressKHR(device->device, &accelerationDeviceAddressInfo);

	delete scratchBuffer;
	delete commandBuffer;
}

UnkBlas::~UnkBlas()
{
	if (handle != VK_NULL_HANDLE)
	{
		device->vkDestroyAccelerationStructureKHR(device->device, handle, nullptr);
		handle = VK_NULL_HANDLE;
	}
	
	if (blasBuffer != nullptr) delete blasBuffer;
}
