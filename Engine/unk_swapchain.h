#pragma once

#include <GLFW/glfw3.h>
#include "vk_mem_alloc.h"

#include "unk_device.h"
#include "unk_image.h"
#include "unk_command_buffer.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <set>
#include <cassert>

class UnkSwapchain
{
public:
	UnkDevice* device;
	GLFWwindow* window;
	VkSurfaceKHR* surface;
	VkSwapchainKHR swapchain;
	vector<UnkImage*> images;

	struct Frame
	{
		UnkCommandBuffer* commandBuffer;

		VkImage swapchainImage = VK_NULL_HANDLE;
		VkFence queueSubmitFence = VK_NULL_HANDLE;
		VkSemaphore swapchainAcquireSemaphore = VK_NULL_HANDLE;
		VkSemaphore swapchainReleaseSemaphore = VK_NULL_HANDLE;
	};

	vector<Frame> frames;

	VkSurfaceFormatKHR surfaceFormat{};
	VkPresentModeKHR presentMode{};
	VkExtent2D extent{};
	VkSurfaceCapabilitiesKHR surfaceProperties{};

	UnkSwapchain();

	UnkSwapchain(UnkDevice* device, VkSurfaceKHR* surface, GLFWwindow* window);

	~UnkSwapchain();

	void createSwapchain();

	Frame createFrame();

	void destroyFrame(Frame& frame);

	void resize();

	// image acquisition and presentation

	VkResult acquireImage(uint32_t* index);

	VkResult presentImage(uint32_t* index);
};