#pragma once

#include "unk_buffer.h"

unordered_map<UnkBuffer*, uint32_t> UnkBuffer::bufferMap;

UnkBuffer::UnkBuffer(UnkDevice* device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VmaAllocationCreateFlags flags)
{
	this->device = device;
	this->size = size;

	VkBufferCreateInfo bufferCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage
	};

	VmaAllocationCreateInfo vmaCreateInfo
	{
		.flags = flags,
		.usage = VMA_MEMORY_USAGE_AUTO,
		.requiredFlags = properties
	};
	vmaCreateBuffer(device->allocator, &bufferCreateInfo, &vmaCreateInfo, &handle, &allocation, &base);

	bufferMap[this] = 1;
}

UnkBuffer::UnkBuffer(UnkDevice* device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VmaAllocationCreateFlags flags, void* data, bool staging)
{
	this->device = device;
	this->size = size;

	VkBuffer pStagingBuffer = VK_NULL_HANDLE;
	VmaAllocation pStagingAllocation{};
	VmaAllocationInfo pStagingBase{};

	VkBufferCreateInfo stagingBufferCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
	};

	VmaAllocationCreateInfo vmaStagingCreateInfo
	{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
		.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	};

	if (!staging)
	{
		vmaCreateBuffer(device->allocator, &stagingBufferCreateInfo, &vmaStagingCreateInfo, &pStagingBuffer, &pStagingAllocation, &pStagingBase);

		if (!pStagingBase.pMappedData) throw runtime_error("Could not map staging buffer");
		memcpy(pStagingBase.pMappedData, data, size);
	}
	else
	{
		vmaCreateBuffer(device->allocator, &stagingBufferCreateInfo, &vmaStagingCreateInfo, &stagingHandle, &stagingAllocation, &stagingBase);

		if (!stagingBase.pMappedData) throw runtime_error("Could not map staging buffer");
		memcpy(stagingBase.pMappedData, data, size);
	}

	UnkCommandBuffer* commandBuffer = new UnkCommandBuffer(device, UnkCommandBuffer::TRANSFER, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	VkBufferCopy copyRegion
	{
		.size = size
	};

	VkBufferCreateInfo bufferCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT
	};

	VmaAllocationCreateInfo vmaCreateInfo
	{
		.flags = flags,
		.usage = VMA_MEMORY_USAGE_AUTO,
		.requiredFlags = properties | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	};
	vmaCreateBuffer(device->allocator, &bufferCreateInfo, &vmaCreateInfo, &handle, &allocation, &base);

	if (!staging)
	{
		vkCmdCopyBuffer(commandBuffer->handle, pStagingBuffer, handle, 1, &copyRegion);

		commandBuffer->endCommand(true);

		delete commandBuffer;

		vmaDestroyBuffer(device->allocator, pStagingBuffer, pStagingAllocation);
	}
	else
	{
		vkCmdCopyBuffer(commandBuffer->handle, stagingHandle, handle, 1, &copyRegion);

		commandBuffer->endCommand(true);

		delete commandBuffer;
	}

	bufferMap[this] = 1;
}


void UnkBuffer::copy(UnkBuffer* other)
{
	UnkCommandBuffer* commandBuffer = new UnkCommandBuffer(device, UnkCommandBuffer::TRANSFER, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	VkBufferCopy copyRegion
	{
		.size = other->size
	};
	vkCmdCopyBuffer(commandBuffer->handle, other->handle, this->handle, 1, &copyRegion);

	commandBuffer->endCommand(true);

	delete commandBuffer;
}

void UnkBuffer::stage()
{
	if (stagingAllocation == VK_NULL_HANDLE) return;

	UnkCommandBuffer* commandBuffer = new UnkCommandBuffer(device, UnkCommandBuffer::TRANSFER, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	VkBufferCopy copyRegion
	{
		.size = this->size
	};
	vkCmdCopyBuffer(commandBuffer->handle, stagingHandle, handle, 1, &copyRegion);

	commandBuffer->endCommand(true);

	delete commandBuffer;
}

UnkBuffer::~UnkBuffer()
{
	if (allocation != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(device->allocator, handle, allocation);
	}

	if (stagingAllocation != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(device->allocator, stagingHandle, stagingAllocation);
	}

	UnkBuffer::bufferMap.erase(this);
}
