#pragma once

#include "unk_swapchain.h"

UnkSwapchain::UnkSwapchain()
{
}

UnkSwapchain::UnkSwapchain(UnkDevice* device, VkSurfaceKHR* surface, GLFWwindow* window)
{
	this->device = device;
	this->window = window;
	this->surface = surface;

	createSwapchain();
}

void UnkSwapchain::createSwapchain()
{
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->gpu, *surface, &surfaceProperties) != VK_SUCCESS)
	{
		throw runtime_error("physical device capabilities not found");
	}

	// find best surface format
	uint32_t surfaceFormatCount;
	vector<VkSurfaceFormatKHR> surfaceFormats;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device->gpu, *surface, &surfaceFormatCount, nullptr);
	assert(surfaceFormatCount > 0);
	surfaceFormats.resize(surfaceFormatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device->gpu, *surface, &surfaceFormatCount, surfaceFormats.data());

	surfaceFormat = surfaceFormats[0];
	for (const auto& availableSurfaceFormat : surfaceFormats)
	{
		if (availableSurfaceFormat.format == VK_FORMAT_R8G8B8_SRGB && availableSurfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			surfaceFormat = availableSurfaceFormat;
		}
	}

	// find best present mode (vsync)
	uint32_t presentModeCount;
	vector<VkPresentModeKHR> presentModes;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device->gpu, *surface, &presentModeCount, nullptr);
	assert(presentModeCount > 0);
	presentModes.resize(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device->gpu, *surface, &presentModeCount, presentModes.data());

	presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (const auto& availablePresentMode : presentModes)
	{
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			presentMode = availablePresentMode;
		}
	}

	// find best swapchain image dimensions
	if (surfaceProperties.currentExtent.width == 0xFFFFFFFF)
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		extent.width = std::clamp(static_cast<uint32_t>(width), surfaceProperties.minImageExtent.width, surfaceProperties.maxImageExtent.width);
		extent.height = std::clamp(static_cast<uint32_t>(height), surfaceProperties.minImageExtent.height, surfaceProperties.maxImageExtent.height);
	}
	else
	{
		extent = surfaceProperties.currentExtent;
	}

	// find best swapchain image count
	uint32_t desiredSwapchainImageCount = surfaceProperties.minImageCount + 1;
	if ((surfaceProperties.maxImageCount > 0) && (desiredSwapchainImageCount > surfaceProperties.maxImageCount))
	{
		desiredSwapchainImageCount = surfaceProperties.maxImageCount;
	}

	// find best swapchain image transform
	VkSurfaceTransformFlagBitsKHR preTransform = surfaceProperties.currentTransform;
	if (surfaceProperties.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}

	// find best alpha composite type
	VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	if (surfaceProperties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
	{
		compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	}

	// find best image sharing mode
	VkSharingMode imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vector<uint32_t> queueIndices;
	set<uint32_t> queueSet;
	queueSet.insert(device->queues.graphics);
	queueSet.insert(device->queues.compute);
	queueSet.insert(device->queues.transfer);

	for (uint32_t index : queueSet)
	{
		queueIndices.push_back(index);
	}

	VkSwapchainKHR oldSwapchain = swapchain;

	// create swapchain
	VkSwapchainCreateInfoKHR swapchainCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = *surface,
		.minImageCount = desiredSwapchainImageCount,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.imageSharingMode = imageSharingMode,
		.queueFamilyIndexCount = static_cast<uint32_t>(queueSet.size()),
		.pQueueFamilyIndices = queueIndices.data(),
		.preTransform = preTransform,
		.compositeAlpha = compositeAlpha,
		.presentMode = presentMode,
		.clipped = true,
		.oldSwapchain = oldSwapchain
	};

	if (vkCreateSwapchainKHR(device->device, &swapchainCreateInfo, nullptr, &swapchain) != VK_SUCCESS)
	{
		throw runtime_error("could not create swapchain");
	}

	if (oldSwapchain != VK_NULL_HANDLE)
	{
		for (UnkImage* image : images)
		{
			delete image;
		}
		images.clear();

		for (auto& frame : frames)
		{
			destroyFrame(frame);
		}

		vkDestroySwapchainKHR(device->device, oldSwapchain, nullptr);
	}

	// retrieve images from swapchain
	uint32_t swapchainImageCount;
	vector<VkImage> swapchainImages;
	vkGetSwapchainImagesKHR(device->device, swapchain, &swapchainImageCount, nullptr);
	assert(swapchainImageCount > 0);
	swapchainImages.resize(swapchainImageCount);
	vkGetSwapchainImagesKHR(device->device, swapchain, &swapchainImageCount, swapchainImages.data());

	frames.clear();
	frames.resize(static_cast<uint32_t>(swapchainImageCount));

	for (uint32_t i = 0; i < swapchainImageCount; i++)
	{
		images.push_back(new UnkImage(device, swapchainImages[i], surfaceFormat.format, true));
		frames[i] = createFrame();
	}
}

UnkSwapchain::Frame UnkSwapchain::createFrame()
{
	Frame frame;

	VkFenceCreateInfo fenceInfo
	{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};
	vkCreateFence(device->device, &fenceInfo, nullptr, &frame.queueSubmitFence);
	frame.commandBuffer = new UnkCommandBuffer(device, UnkCommandBuffer::GRAPHICS, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

	return frame;
}

void UnkSwapchain::destroyFrame(Frame& frame)
{
	if (frame.queueSubmitFence != VK_NULL_HANDLE)
	{
		vkDestroyFence(device->device, frame.queueSubmitFence, nullptr);
		frame.queueSubmitFence = VK_NULL_HANDLE;
	}

	if (frame.swapchainAcquireSemaphore != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(device->device, frame.swapchainAcquireSemaphore, nullptr);
		frame.swapchainAcquireSemaphore = VK_NULL_HANDLE;
	}

	if (frame.swapchainReleaseSemaphore != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(device->device, frame.swapchainReleaseSemaphore, nullptr);
		frame.swapchainReleaseSemaphore = VK_NULL_HANDLE;
	}
}

void UnkSwapchain::resize()
{
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(device->device);

	createSwapchain();
}


VkResult UnkSwapchain::acquireImage(uint32_t* index)
{
	VkSemaphore imageAcquiredSemaphore;
	VkSemaphoreCreateInfo info =
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};
	vkCreateSemaphore(device->device, &info, nullptr, &imageAcquiredSemaphore);

	VkResult res = vkAcquireNextImageKHR(device->device, swapchain, UINT64_MAX, imageAcquiredSemaphore, VK_NULL_HANDLE, index);
	if (res != VK_SUCCESS)
	{
		vkDestroySemaphore(device->device, imageAcquiredSemaphore, nullptr);
		return res;
	}

	if (frames[*index].queueSubmitFence != VK_NULL_HANDLE)
	{
		vkWaitForFences(device->device, 1, &frames[*index].queueSubmitFence, VK_TRUE, UINT64_MAX);
		vkResetFences(device->device, 1, &frames[*index].queueSubmitFence);
	}

	if (frames[*index].swapchainAcquireSemaphore != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(device->device, frames[*index].swapchainAcquireSemaphore, nullptr);
	}

	frames[*index].swapchainAcquireSemaphore = imageAcquiredSemaphore;

	return VK_SUCCESS;
}

VkResult UnkSwapchain::presentImage(uint32_t* index)
{
	VkPresentInfoKHR present
	{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &frames[*index].swapchainReleaseSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &swapchain,
		.pImageIndices = index
	};

	return vkQueuePresentKHR(device->getQueue(device->queues.graphics), &present);
}

UnkSwapchain::~UnkSwapchain()
{
	for (UnkImage* image : images)
	{
		delete image;
	}

	for (auto& frame : frames)
	{
		destroyFrame(frame);
	}

	vkDestroySwapchainKHR(device->device, swapchain, nullptr);
}