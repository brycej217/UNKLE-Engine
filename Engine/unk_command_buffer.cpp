#pragma once
#include "unk_command_buffer.h"

UnkCommandBuffer::UnkCommandBuffer(UnkDevice* device, Index type, VkCommandBufferLevel level, bool begin)
{
	this->device = device;

	if (type == GRAPHICS)
	{
		this->commandPool = &device->graphicsPool;
		this->queueIndex = device->queues.graphics;
	}
	if (type == COMPUTE)
	{
		this->commandPool = &device->computePool;
		this->queueIndex = device->queues.compute;
	}
	if (type == TRANSFER) 
	{
		this->commandPool = &device->transferPool;
		this->queueIndex = device->queues.transfer;
	}

	VkCommandBufferAllocateInfo commandBufferInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = *commandPool,
		.level = level,
		.commandBufferCount = 1
	};
	vkAllocateCommandBuffers(device->device, &commandBufferInfo, &handle);

	if (begin)
	{
		beginCommand();
	}
}

UnkCommandBuffer::~UnkCommandBuffer()
{
	vkFreeCommandBuffers(device->device, *commandPool, 1, &handle);
}

void UnkCommandBuffer::beginCommand()
{
	VkCommandBufferBeginInfo beginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(handle, &beginInfo);
}

void UnkCommandBuffer::endCommand(bool submit)
{
	vkEndCommandBuffer(handle);

	if (!submit) return;
	
	VkSubmitInfo submitInfo
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &handle
	};
	VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkFence fence;
	vkCreateFence(device->device, &fenceInfo, nullptr, &fence);
	VK_CHECK(vkQueueSubmit(device->getQueue(queueIndex), 1, &submitInfo, fence));
	VK_CHECK(vkWaitForFences(device->device, 1, &fence, VK_TRUE, UINT64_MAX));
	vkDestroyFence(device->device, fence, nullptr);
}
