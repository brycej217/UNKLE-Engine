#pragma once

#include "unk_device.h"
#include "unk_swapchain.h"

#include "rasterizer.h"
#include "raytracer.h"
#include "structs.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <vulkan/vulkan.h>
#include <vector>
#include <set>

using namespace std;
using namespace glm;

class Renderer
{
public:
	Renderer(GLFWwindow* window, uint32_t width, uint32_t height);

	~Renderer();

	void render(Camera camera, float deltaTime);

	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

	GLFWwindow* window = nullptr;
	VkSurfaceKHR surface = VK_NULL_HANDLE;

	UnkDevice* device;
	UnkSwapchain* swapchain;

	vector<Pipeline*> pipelines;
	uint32_t currPipeline;
	
	DeviceResources deviceResources;

	// initialization

	void createInstance();

	VkPhysicalDevice selectPhysicalDevice(vector<const char*> requiredExtensions);

	void createDevice();

	void switchPipeline();

	// pipeline 

	void createPipeline();

	void createDeviceResources();

	void updateInstances(Camera camera, float deltaTime);

	void createVertexBuffers(vector<Vertex>& vertices, vector<uint32_t>& indices);

	void createTexture(void* data, uint32_t width, uint32_t height);
};