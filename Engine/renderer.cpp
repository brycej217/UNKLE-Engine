#define GLFW_INCLUDE_VULKAN
#define VMA_IMPLEMENTATION
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define TINYOBJLOADER_IMPLEMENTATION
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/hash.hpp>
#include "renderer.h"
#include "utils.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <tiny_obj_loader.h>

#include <vector>
#include <algorithm>
#include <array>
#include <fstream>
#include <chrono>
#include <unordered_map>

using namespace std;
using namespace glm;
using namespace chrono;

#ifndef NDEBUG
#define ENABLE_DEBUG
#define ENABLE_VALIDATION_LAYERS
#endif

#if defined(ENABLE_DEBUG) || defined(ENABLE_VALIDATION_LAYERS)
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		cerr << "validation layer: " << pCallbackData->pMessage << endl;
		__debugbreak();
	}
	return VK_FALSE;
}
#endif

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.position) ^
				(hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}

void Renderer::createInstance()
{
	LOG("Initializing vulkan instance");

	// query instance extensions
	vector<const char*> requiredInstanceExtensions;

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	for (uint32_t i = 0; i < glfwExtensionCount; i++)
	{
		requiredInstanceExtensions.push_back(glfwExtensions[i]);
	}

	auto availableInstanceExtensions = VK_ENUMERATE<VkExtensionProperties>(vkEnumerateInstanceExtensionProperties, nullptr);

#if defined(ENABLE_DEBUG) || defined(ENABLE_VALIDATION_LAYERS)
	requiredInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	if (!VK_VALIDATE(requiredInstanceExtensions, availableInstanceExtensions, getExtensionName))
	{
		throw runtime_error("Required instance extensions are missing");
	}

	// query instance layers
	vector<const char*> requestedInstanceLayers{};

#if defined(ENABLE_DEBUG) || defined(ENABLE_VALIDATION_LAYERS)
	char const* validationLayer = "VK_LAYER_KHRONOS_validation";

	requestedInstanceLayers.push_back(validationLayer);

	auto supportedInstanceLayers = VK_ENUMERATE<VkLayerProperties>(vkEnumerateInstanceLayerProperties);

	if (!VK_VALIDATE(requestedInstanceLayers, supportedInstanceLayers, getLayerName))
	{
		throw runtime_error("Requested validation layers not available");
	}
#endif

	VkApplicationInfo app
	{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Ray-Tracer",
		.pEngineName = "Ray-Tracer Engine",
		.apiVersion = VK_API_VERSION_1_1
	};

	VkInstanceCreateInfo appCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app,
		.enabledLayerCount = static_cast<uint32_t>(requestedInstanceLayers.size()),
		.ppEnabledLayerNames = requestedInstanceLayers.data(),
		.enabledExtensionCount = static_cast<uint32_t>(requiredInstanceExtensions.size()),
		.ppEnabledExtensionNames = requiredInstanceExtensions.data()
	};

	// debug messenger creation info
#if defined(ENABLE_DEBUG) || defined(ENABLE_VALIDATION_LAYERS)
	VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo =
	{
	.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
	.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
	.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
	.pfnUserCallback = debugCallback
	};

	appCreateInfo.pNext = &debugUtilsCreateInfo;
#endif

	VK_CHECK(vkCreateInstance(&appCreateInfo, nullptr, &instance)); // create instance

	// create debug messenger
#if defined(ENABLE_DEBUG) || defined(ENABLE_VALIDATION_LAYERS)
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		VK_CHECK(func(instance, &debugUtilsCreateInfo, nullptr, &debugMessenger));
	}
#endif
}

void Renderer::createDevice()
{
	LOG("Initializing vulkan device");

	auto gpus = VK_ENUMERATE<VkPhysicalDevice>(vkEnumeratePhysicalDevices, instance);

	if (gpus.empty())
	{
		throw runtime_error("No physical device found");
	}

	vector<const char*> requiredDeviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME };

	// find suitable gpu
	for (const auto& gpu : gpus)
	{
		// check gpu queue family support
		bool foundGraphics = false, foundPresent = false, foundTransfer = false;
		auto availableQueueFamilies = VK_ENUMERATE<VkQueueFamilyProperties>(vkGetPhysicalDeviceQueueFamilyProperties, gpu);
		for (uint32_t i = 0; i < static_cast<uint32_t>(availableQueueFamilies.size()); i++)
		{
			if (!foundGraphics && availableQueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				queues.graphicsIndex = i;
				foundGraphics = true;
			}
			else if (!foundTransfer && availableQueueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				queues.transferIndex = i;
				foundTransfer = true;
			}

			if (!foundPresent)
			{
				VkBool32 presentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &presentSupport);

				if (presentSupport)
				{
					queues.presentIndex = i;
					foundPresent = true;
				}
			}

			if (foundGraphics && foundPresent && foundTransfer)
			{
				break;
			}
		}

		if (queues.graphicsIndex != UINT32_MAX)
		{
			queues.uniqueIndices.insert(queues.graphicsIndex);
		}
		if (queues.presentIndex != UINT32_MAX)
		{
			queues.uniqueIndices.insert(queues.presentIndex);
		}
		if (queues.transferIndex != UINT32_MAX)
		{
			queues.uniqueIndices.insert(queues.transferIndex);
		}

		// check gpu extension availability
		auto availableDeviceExtensions = VK_ENUMERATE<VkExtensionProperties>(vkEnumerateDeviceExtensionProperties, gpu, nullptr);
		if (!VK_VALIDATE(requiredDeviceExtensions, availableDeviceExtensions, getExtensionName)) continue;

		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(gpu, &supportedFeatures);

		if (!supportedFeatures.samplerAnisotropy) continue;
		if (!supportedFeatures.multiDrawIndirect) continue;

		this->gpu = gpu;
	}

	vector<VkDeviceQueueCreateInfo> queueInfos;

	const float queuePriority = 1.0f;
	for (uint32_t queueFamilyIndex : queues.uniqueIndices)
	{
		VkDeviceQueueCreateInfo queueInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = queueFamilyIndex,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority
		};

		queueInfos.push_back(queueInfo);
	}

	VkPhysicalDeviceDescriptorIndexingFeatures indexing
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
		.shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
		.descriptorBindingPartiallyBound = VK_TRUE,
		.descriptorBindingVariableDescriptorCount = VK_TRUE,
		.runtimeDescriptorArray = VK_TRUE,
	};
	VkPhysicalDeviceVulkan11Features features11
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
		.pNext = &indexing,
		.shaderDrawParameters = VK_TRUE
	};
	VkPhysicalDeviceFeatures2 features2
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &features11,
		.features = 
		{
			.multiDrawIndirect = VK_TRUE,
			.drawIndirectFirstInstance = VK_TRUE,
			.samplerAnisotropy = VK_TRUE
		}
	};

	VkDeviceCreateInfo deviceInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &features2,
		.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size()),
		.pQueueCreateInfos = queueInfos.data(),
		.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
		.ppEnabledExtensionNames = requiredDeviceExtensions.data(),
		.pEnabledFeatures = nullptr
	};

	VK_CHECK(vkCreateDevice(gpu, &deviceInfo, nullptr, &device));

	vkGetDeviceQueue(device, queues.graphicsIndex, 0, &queues.graphicsQueue);
	vkGetDeviceQueue(device, queues.presentIndex, 0, &queues.presentQueue);
	vkGetDeviceQueue(device, queues.transferIndex, 0, &queues.transferQueue);
}

void Renderer::createSwapchain()
{
	LOG("Initializing swapchain");

	VkSurfaceCapabilitiesKHR surfaceProperties;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &surfaceProperties));

	// find best surface format
	VkSurfaceFormatKHR surfaceFormat;
	auto availableSurfaceFormats = VK_ENUMERATE<VkSurfaceFormatKHR>(vkGetPhysicalDeviceSurfaceFormatsKHR, gpu, surface);
	surfaceFormat = availableSurfaceFormats[0];
	for (const auto& availableSurfaceFormat : availableSurfaceFormats)
	{
		if (availableSurfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableSurfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			surfaceFormat = availableSurfaceFormat;
		}
	}
	swapchain.dimensions.format = surfaceFormat.format;

	// find best present mode (vsync)
	VkPresentModeKHR presentMode;
	presentMode = VK_PRESENT_MODE_FIFO_KHR;
	auto availablePresentModes = VK_ENUMERATE<VkPresentModeKHR>(vkGetPhysicalDeviceSurfacePresentModesKHR, gpu, surface);
	for (const auto& availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			presentMode = availablePresentMode;
		}
	}

	// find best swapchain image dimensions
	VkExtent2D swapchainExtent{};
	if (surfaceProperties.currentExtent.width == 0xFFFFFFFF)
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		swapchainExtent.width = std::clamp(static_cast<uint32_t>(width), surfaceProperties.minImageExtent.width, surfaceProperties.maxImageExtent.width);
		swapchainExtent.height = std::clamp(static_cast<uint32_t>(height), surfaceProperties.minImageExtent.height, surfaceProperties.maxImageExtent.height);
	}
	else
	{
		swapchainExtent = surfaceProperties.currentExtent;
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
	if (static_cast<uint32_t>(queues.uniqueIndices.size()) > 1)
	{
		imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	}
	vector<uint32_t> queueFamilyIndices(queues.uniqueIndices.begin(), queues.uniqueIndices.end());

	VkSwapchainKHR oldSwapchain = swapchain.swapchain;

	// create swapchain
	VkSwapchainCreateInfoKHR info
	{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = desiredSwapchainImageCount,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = swapchainExtent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = imageSharingMode,
		.queueFamilyIndexCount = static_cast<uint32_t>(queues.uniqueIndices.size()),
		.pQueueFamilyIndices = queueFamilyIndices.data(),
		.preTransform = preTransform,
		.compositeAlpha = compositeAlpha,
		.presentMode = presentMode,
		.clipped = true,
		.oldSwapchain = oldSwapchain
	};

	VK_CHECK(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain.swapchain));

	if (oldSwapchain != VK_NULL_HANDLE)
	{
		for (VkImageView imageView : swapchain.imageViews)
		{
			vkDestroyImageView(device, imageView, nullptr);
		}

		for (auto& perFrame : perFrame)
		{
			destroyPerFrame(perFrame);
		}

		swapchain.imageViews.clear();
		vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
	}

	swapchain.dimensions = { swapchainExtent.width, swapchainExtent.height, surfaceFormat.format };

	// retrieve images from swapchain
	auto swapchainImages = VK_ENUMERATE<VkImage>(vkGetSwapchainImagesKHR, device, swapchain.swapchain);
	perFrame.clear();
	perFrame.resize(static_cast<uint32_t>(swapchainImages.size()));
	for (uint32_t i = 0; i < static_cast<uint32_t>(swapchainImages.size()); i++)
	{
		// create image views
		VkImageViewCreateInfo viewInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchainImages[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = surfaceFormat.format,
			.subresourceRange =
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};

		VkImageView imageView;
		VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &imageView));
		swapchain.imageViews.push_back(imageView);

		// create per frame resources
		createPerFrame(perFrame[i]);
	}
}

void Renderer::resizeSwapchain()
{
	LOG("Resizing swapchain");

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(device);

	pipeline->destroyResizeResources();

	createSwapchain();
	pipeline->createFramebuffers();
}

void Renderer::createPerFrame(PerFrame& perFrame)
{
	VkFenceCreateInfo fenceInfo
	{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};
	VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &perFrame.queueSubmitFence));

	VkCommandPoolCreateInfo commandPoolInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		.queueFamilyIndex = static_cast<uint32>(queues.graphicsIndex)
	};
	VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &perFrame.commandPool));

	VkCommandBufferAllocateInfo commandBufferInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = perFrame.commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferInfo, &perFrame.commandBuffer));
}

void Renderer::destroyPerFrame(PerFrame& perFrame)
{
	if (perFrame.queueSubmitFence != VK_NULL_HANDLE)
	{
		vkDestroyFence(device, perFrame.queueSubmitFence, nullptr);
		perFrame.queueSubmitFence = VK_NULL_HANDLE;
	}

	if (perFrame.commandBuffer != VK_NULL_HANDLE)
	{
		vkFreeCommandBuffers(device, perFrame.commandPool, 1, &perFrame.commandBuffer);
		perFrame.commandBuffer = VK_NULL_HANDLE;
	}

	if (perFrame.commandPool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(device, perFrame.commandPool, nullptr);
		perFrame.commandPool = VK_NULL_HANDLE;
	}

	if (perFrame.swapchainAcquireSemaphore != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(device, perFrame.swapchainAcquireSemaphore, nullptr);
		perFrame.swapchainAcquireSemaphore = VK_NULL_HANDLE;
	}

	if (perFrame.swapchainReleaseSemaphore != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(device, perFrame.swapchainReleaseSemaphore, nullptr);
		perFrame.swapchainReleaseSemaphore = VK_NULL_HANDLE;
	}
}

/*
* DRAW CALL RESOURCE CREATION
* Universal buffers must be created after scene loading
*/

/*
* Creates pipeline object
* Called by engine after scene has been loaded and draw call resources have been created
*/
void Renderer::createPipeline()
{
	createPipelineFeed();
	pipeline = new Rasterizer(&device, &gpu, allocator, &swapchain, &pipelineFeed);
}

void Renderer::createPipelineFeed()
{
	LOG("Create pipeline feed");

	// create instance SSBO
	VkDeviceSize instancesSize = pipelineFeed.instances.size() * sizeof(Instance);
	allocator->createDeviceLocalBuffer(
		pipelineFeed.instances.data(),
		instancesSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		0,
		pipelineFeed.instanceSSBO,
		&pipelineFeed.instanceInfo
	);

	// create transform SSBO
	VkDeviceSize transformsSize = pipelineFeed.transforms.size() * sizeof(MVP);
	allocator->createDeviceLocalBuffer(
		pipelineFeed.transforms.data(),
		transformsSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		0,
		pipelineFeed.transformStagingBuffer,
		pipelineFeed.transformSSBO,
		&pipelineFeed.transformInfo
	);

	// create light SSBO
	VkDeviceSize lightsSize = pipelineFeed.lights.size() * sizeof(Light);
	allocator->createDeviceLocalBuffer(
		pipelineFeed.lights.data(),
		lightsSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		0,
		pipelineFeed.lightStagingBuffer,
		pipelineFeed.lightSSBO,
		&pipelineFeed.lightInfo
	);

	// create indirect command SSBO
	for (int i = 0; i < meshes.size(); i++)
	{
		pipelineFeed.drawCommands.push_back(meshes[i].getDrawCommand());
	}
	VkDeviceSize drawCommandBufferSize = pipelineFeed.drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand);
	VmaAllocationInfo drawCommandInfo;

	allocator->createDeviceLocalBuffer(
		pipelineFeed.drawCommands.data(),
		drawCommandBufferSize,
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
		0,
		pipelineFeed.drawCommandBuffer,
		&drawCommandInfo
	);

	// texture sampler
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(gpu, &properties);
	VkSamplerCreateInfo samplerInfo
	{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias = 0.0f,
		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE
	};
	VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &pipelineFeed.textureImageSampler));
}

void Renderer::createVertexBuffer(vector<Vertex>& vertices, vector<uint32_t>& indices, VkBuffer& vertexBuffer, VkBuffer& indexBuffer)
{
	const VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
	allocator->createDeviceLocalBuffer(
		(void*)vertices.data(),
		vertexBufferSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		0,
		vertexBuffer,
		nullptr);

	const VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
	allocator->createDeviceLocalBuffer(
		(void*)indices.data(),
		indexBufferSize,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		0,
		indexBuffer,
		nullptr);
}

void Renderer::createTexture(void* data, uint32_t imageSize, uint32_t texWidth, uint32_t texHeight)
{
	LOG("Initializing texture");

	// create texture image
	VkImage textureImage;
	allocator->createTextureImage(
		data,
		imageSize,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY,
		texWidth,
		texHeight,
		textureImage,
		nullptr);
	pipelineFeed.textureImages.push_back(textureImage);

	// create image views and samplers
	VkImageView textureImageView;
	VkImageViewCreateInfo viewInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = textureImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_SRGB,
		.subresourceRange =
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};
	VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &textureImageView));
	pipelineFeed.textureImageViews.push_back(textureImageView);
}

void Renderer::updateInstances(Camera camera, float deltaTime)
{
	static float totalDelta;
	totalDelta += deltaTime;

	VkDeviceSize size = pipelineFeed.transforms.size() * sizeof(MVP);
	uint8_t* base = static_cast<uint8_t*>(pipelineFeed.transformInfo.pMappedData);

	for (int i = 0; i < pipelineFeed.transforms.size(); i++)
	{
		MVP mvp
		{
			.model = pipelineFeed.transforms[i].model,
			.view = camera.getViewMatrix(),
			.proj = perspective(radians(45.0f), swapchain.dimensions.width / (float)swapchain.dimensions.height, 0.1f, 100.0f)
		};
		mvp.proj[1][1] *= -1;

		memcpy(base + i * sizeof(MVP), &mvp, sizeof(MVP));
	}

	allocator->copyBuffer(size, pipelineFeed.transformStagingBuffer, pipelineFeed.transformSSBO);
}

/*
* RENDERING
*/

VkResult Renderer::acquireImage(uint32_t* imageIndex)
{
	VkSemaphore imageAcquiredSemaphore;
	VkSemaphoreCreateInfo info =
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};
	VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &imageAcquiredSemaphore));

	VkResult res = vkAcquireNextImageKHR(device, swapchain.swapchain, UINT64_MAX, imageAcquiredSemaphore, VK_NULL_HANDLE, imageIndex);
	if (res != VK_SUCCESS)
	{
		vkDestroySemaphore(device, imageAcquiredSemaphore, nullptr);
		return res;
	}

	if (perFrame[*imageIndex].queueSubmitFence != VK_NULL_HANDLE)
	{
		vkWaitForFences(device, 1, &perFrame[*imageIndex].queueSubmitFence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &perFrame[*imageIndex].queueSubmitFence);
	}

	if (perFrame[*imageIndex].commandPool != VK_NULL_HANDLE)
	{
		vkResetCommandPool(device, perFrame[*imageIndex].commandPool, 0);
	}

	if (perFrame[*imageIndex].swapchainAcquireSemaphore != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(device, perFrame[*imageIndex].swapchainAcquireSemaphore, nullptr);
	}

	perFrame[*imageIndex].swapchainAcquireSemaphore = imageAcquiredSemaphore;

	return VK_SUCCESS;
}

VkResult Renderer::presentImage(uint32_t imageIndex)
{
	VkPresentInfoKHR present
	{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &perFrame[imageIndex].swapchainReleaseSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &swapchain.swapchain,
		.pImageIndices = &imageIndex
	};

	return vkQueuePresentKHR(queues.graphicsQueue, &present);
}

void Renderer::render(Camera camera, float deltaTime)
{
	uint32_t index;

	auto res = acquireImage(&index);

	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
	{
		resizeSwapchain();
		res = acquireImage(&index);
	}

	if (res != VK_SUCCESS)
	{
		vkQueueWaitIdle(queues.graphicsQueue);
		return;
	}

	updateInstances(camera, deltaTime);

	pipeline->draw(index, perFrame, &queues.graphicsQueue, meshes);
	
	res = presentImage(index);

	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
	{
		resizeSwapchain();
	}
	else if (res != VK_SUCCESS)
	{
		LOG("Failed to present swapchain image");
	}
}

Renderer::Renderer(GLFWwindow* window, uint32_t width, uint32_t height)
{
	this->window = window;
	swapchain.dimensions.width = width;
	swapchain.dimensions.height = height;
	
	createInstance();
	
	VK_CHECK(glfwCreateWindowSurface(instance, this->window, nullptr, &surface));
	
	createDevice();

	allocator = new Allocator(gpu, device, instance, queues);

	createSwapchain();
}

Renderer::~Renderer()
{
	LOG("Destroying renderer");

	vkDeviceWaitIdle(device);

	for (auto& perFrame : perFrame)
	{
		destroyPerFrame(perFrame);
	}
	perFrame.clear();

	swapchain.cleanup(&device);

	pipelineFeed.cleanup(&device);

	delete pipeline;

	delete allocator;
	
	if (device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(device, nullptr);
	}

	if (surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(instance, surface, nullptr);
	}

	if (debugCallback != VK_NULL_HANDLE)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			func(instance, debugMessenger, nullptr);
		}
	}

	vkDestroyInstance(instance, nullptr);
}