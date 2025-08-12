#pragma once

#include "allocator.h"
#include "rasterizer.h"
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

	VkPhysicalDevice gpu = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;

	Queues queues;

	Rasterizer* pipeline = nullptr;
	Allocator* allocator = nullptr;

	// swapchain data
	Swapchain swapchain;

	vector<PerFrame> perFrame;
	
	// draw call resources
	vector<Mesh> meshes;
	PipelineFeed pipelineFeed;

	// initialization

	void createInstance();

	void createDevice();

	void createPerFrame(PerFrame& perFrame);

	void destroyPerFrame(PerFrame& perFrame);

	void createSwapchain();


	// utility

	void resizeSwapchain();

	// pipeline 

	void createPipeline();

	void createPipelineFeed();

	void updateInstances(Camera camera, float deltaTime);

	// draw feed resource creation

	void createVertexBuffer(vector<Vertex>& vertices, vector<uint32_t>& indices, VkBuffer& vertexBuffer, VkBuffer& indexBuffer);

	void createTexture(void* data, uint32_t imageSize, uint32_t texWidth, uint32_t texHeight);

	// rendering

	VkResult acquireImage(uint32_t* imageIndex);

	VkResult presentImage(uint32_t imageIndex);
};