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
		cerr << "validation layer: " << pCallbackData->pMessage << "\n" << endl;
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

Renderer::Renderer(GLFWwindow* window, uint32_t width, uint32_t height)
{
	this->window = window;

	createInstance();

	VK_CHECK(glfwCreateWindowSurface(instance, this->window, nullptr, &surface));

	createDevice();

	this->swapchain = new UnkSwapchain(device, &surface, window);
}

void Renderer::createInstance()
{
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

VkPhysicalDevice Renderer::selectPhysicalDevice(vector<const char*> requiredExtensions)
{
	// get available gpus
	uint32_t gpuCount;
	vector<VkPhysicalDevice> gpus;
	vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
	assert(gpuCount > 0);
	gpus.resize(gpuCount);
	vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data());


	// find suitable gpu
	for (const auto& gpu : gpus)
	{
		// check gpu queue family support
		bool graphics = false, compute = false, transfer = false;

		uint32_t queueFamilyCount;
		vector<VkQueueFamilyProperties> queueFamilyProperties;
		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
		assert(queueFamilyCount > 0);
		queueFamilyProperties.resize(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueFamilyProperties.data());

		for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
		{
			if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
			{
				compute = true;
			}
			else if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
			{
				transfer = true;
			}
			else if (!graphics && queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				graphics = true;
			}

			if (graphics && compute && transfer)
			{
				break;
			}
		}

		// check gpu extension availability
		uint32_t extensionCount;
		vector<VkExtensionProperties> extensionProperties;
		vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, nullptr);
		assert(extensionCount > 0);
		extensionProperties.resize(extensionCount);
		vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, extensionProperties.data());

		// query if required extension capabilities are met
		set<string> requiredSet;

		for (auto requiredExtension : requiredSet)
		{
			requiredSet.insert(requiredExtension);
			for (auto& availableExtension : extensionProperties)
			{
				if (strcmp(availableExtension.extensionName, requiredExtension.c_str()) == 0)
				{
					requiredSet.erase(requiredExtension);
					break;
				}
			}
		}

		if (!requiredSet.empty()) continue;

		// end of pNext chain
		VkPhysicalDeviceRayTracingValidationFeaturesNV validationProbe
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV,
		};

		// accelerationStructure -> validation
		VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureProbe
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
			.pNext = &validationProbe,
		};

		// rayTracingPipeline -> accelerationStructure
		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineProbe
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
			.pNext = &accelerationStructureProbe,
		};

		// bufferDeviceAddress -> rayTracingPipeline
		VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressProbe
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
			.pNext = &rayTracingPipelineProbe,
		};

		// descriptorIndexing -> bufferDeviceAddress
		VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingProbe
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
			.pNext = &bufferDeviceAddressProbe,
		};

		// features11 -> descriptorIndexing
		VkPhysicalDeviceVulkan11Features probe11
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
			.pNext = &descriptorIndexingProbe,
		};

		// features2 -> features11
		VkPhysicalDeviceFeatures2 probe2
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = &probe11,
		};

		vkGetPhysicalDeviceFeatures2(gpu, &probe2);

		bool valid =
			probe2.features.samplerAnisotropy &&
			probe2.features.multiDrawIndirect &&
			probe2.features.drawIndirectFirstInstance &&
			probe11.shaderDrawParameters &&
			descriptorIndexingProbe.runtimeDescriptorArray &&
			descriptorIndexingProbe.descriptorBindingPartiallyBound &&
			descriptorIndexingProbe.shaderSampledImageArrayNonUniformIndexing &&
			bufferDeviceAddressProbe.bufferDeviceAddress &&
			rayTracingPipelineProbe.rayTracingPipeline &&
			accelerationStructureProbe.accelerationStructure;

		if (!valid) continue;

		return gpu;
	}
}

void Renderer::createDevice()
{
	vector<const char*> enabledExtensions
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_SPIRV_1_4_EXTENSION_NAME,
		VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
		VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
	};

	VkPhysicalDevice physicalDevice = selectPhysicalDevice(enabledExtensions);

	// end of pNext chain
	VkPhysicalDeviceRayTracingValidationFeaturesNV validation
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV,
		.pNext = nullptr,
		.rayTracingValidation = VK_TRUE
	};

	// accelerationStructure -> validation
	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
		.pNext = &validation,
		.accelerationStructure = VK_TRUE
	};

	// rayTracingPipeline -> accelerationStructure
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
		.pNext = &accelerationStructureFeatures,
		.rayTracingPipeline = VK_TRUE
	};

	// bufferDeviceAddress -> rayTracingPipeline
	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
		.pNext = &rayTracingPipelineFeatures,
		.bufferDeviceAddress = VK_TRUE,
	};

	// descriptorIndexing -> bufferDeviceAddress
	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
		.pNext = &bufferDeviceAddressFeatures,
		.shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
		.descriptorBindingPartiallyBound = VK_TRUE,
		.descriptorBindingVariableDescriptorCount = VK_TRUE,
		.runtimeDescriptorArray = VK_TRUE,
	};

	// features11 -> descriptorIndexing
	VkPhysicalDeviceVulkan11Features features11
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
		.pNext = &descriptorIndexingFeatures,
		.shaderDrawParameters = VK_TRUE
	};

	// features2 -> features11
	VkPhysicalDeviceFeatures2 features2
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &features11,
		.features = 
		{
			.multiDrawIndirect = VK_TRUE,
			.drawIndirectFirstInstance = VK_TRUE,
			.samplerAnisotropy = VK_TRUE,
		}
	};

	VkPhysicalDeviceFeatures deviceFeatures{};
	device = new UnkDevice
	(
		instance,
		physicalDevice, 
		deviceFeatures,
		enabledExtensions, 
		&features2
	);
}

/*
* Creates pipeline object
* Called by engine after scene has been loaded and draw call resources have been created
*/
void Renderer::createPipeline()
{
	createDeviceResources();
	pipelines.push_back(new Rasterizer(device, swapchain, &deviceResources));
	pipelines.push_back(new RayTracer(device, swapchain, &deviceResources));

	currPipeline = 0;
}

void Renderer::switchPipeline()
{
	vkDeviceWaitIdle(device->device);

	currPipeline = (currPipeline + 1) % 2;
}

void Renderer::createDeviceResources()
{
	deviceResources.instanceBuffer = new UnkBuffer
	(
		device,
		deviceResources.instances.size() * sizeof(Instance),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		0,
		0,
		deviceResources.instances.data()
	);
	
	deviceResources.transformBuffer = new UnkBuffer
	(
		device,
		deviceResources.transforms.size() * sizeof(MVP),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		0,
		0,
		deviceResources.transforms.data(),
		true
	);

	deviceResources.lightBuffer = new UnkBuffer
	(
		device,
		deviceResources.lights.size() * sizeof(Light),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		0,
		0,
		deviceResources.lights.data()
	);

	deviceResources.cameraBuffer = new UnkBuffer
	(
		device,
		sizeof(CameraGPU),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
	);

	// create indirect command SSBO
	vector<VkDrawIndexedIndirectCommand> drawCommands;
	for (int i = 0; i < deviceResources.meshes.size(); i++)
	{
		drawCommands.push_back(deviceResources.meshes[i].getDrawCommand());
	}
	VkDeviceSize drawCommandBufferSize = drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand);
	VmaAllocationInfo drawCommandInfo;

	deviceResources.drawCommandBuffer = new UnkBuffer
	(
		device,
		drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand),
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
		0,
		0,
		drawCommands.data()
	);

	// create and set texture sampler
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(device->gpu, &properties);
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
	vkCreateSampler(device->device, &samplerInfo, nullptr, &deviceResources.sampler);

	for (auto& textureImage : deviceResources.textureImages)
	{
		textureImage->sampler = &deviceResources.sampler;
	}
}

void Renderer::createVertexBuffers(vector<Vertex>& vertices, vector<uint32_t>& indices)
{
	deviceResources.vertexBuffer = new UnkBuffer
	(
		device,
		vertices.size() * sizeof(Vertex),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		0,
		0,
		vertices.data()
	);

	deviceResources.indexBuffer = new UnkBuffer
	(
		device,
		indices.size() * sizeof(uint32_t),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		0,
		0,
		indices.data()
	);
}

void Renderer::createTexture(void* data, uint32_t width, uint32_t height)
{
	// create texture image
	UnkImage* textureImage = new UnkImage
	(
		device,
		height,
		width,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		true,
		data
	);
	deviceResources.textureImages.push_back(textureImage);
}

void Renderer::updateInstances(Camera camera, float deltaTime)
{
	static float totalDelta;
	totalDelta += deltaTime;

	CameraGPU camGPU;

	camGPU.viewInv = camera.transform.getWorldMatrix();
	mat4 proj = perspective(radians(45.0f), swapchain->extent.width / (float)swapchain->extent.height, 0.1f, 10000.0f);
	proj[1][1] *= -1;
	camGPU.projInv = inverse(proj);

	VkDeviceSize size = deviceResources.transforms.size() * sizeof(MVP);
	uint8_t* base = static_cast<uint8_t*>(deviceResources.transformBuffer->stagingBase.pMappedData);

	for (int i = 0; i < deviceResources.transforms.size(); i++)
	{
		MVP mvp
		{
			.model = deviceResources.transforms[i].model,
			.view = camera.getViewMatrix(),
			.proj = proj
		};

		memcpy(base + i * sizeof(MVP), &mvp, sizeof(MVP));
	}

	deviceResources.transformBuffer->stage();

	memcpy(deviceResources.cameraBuffer->base.pMappedData, &camGPU, sizeof(CameraGPU));
}

/*
* RENDERING
*/

void Renderer::render(Camera camera, float deltaTime)
{
	uint32_t index;

	auto res = swapchain->acquireImage(&index);

	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
	{
		swapchain->resize();
		pipelines[currPipeline]->handleResize();
		res = swapchain->acquireImage(&index);
	}

	if (res != VK_SUCCESS)
	{
		vkQueueWaitIdle(device->getQueue(device->queues.graphics));
		return;
	}

	updateInstances(camera, deltaTime);

	pipelines[currPipeline]->draw(index);
	
	res = swapchain->presentImage(&index);

	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
	{
		swapchain->resize();
		
		for (auto& pipeline : pipelines)
		{
			pipeline->handleResize();
		}

		res = swapchain->acquireImage(&index);
	}
	else if (res != VK_SUCCESS)
	{
		LOG("Failed to present swapchain image");
	}
}

Renderer::~Renderer()
{
	vkDeviceWaitIdle(device->device);

	for (auto& pipeline : pipelines)
	{
		delete pipeline;
	}
	
	delete swapchain;

	if (surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(instance, surface, nullptr);
	}

	deviceResources.destroy(device);

	delete device;

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