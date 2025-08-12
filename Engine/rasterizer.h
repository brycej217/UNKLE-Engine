#pragma once
#include "pipeline.h"

class Rasterizer : public Pipeline
{
public:
	vector<VkImage> depthImages;
	vector<VkImageView> depthImageViews;

	Rasterizer() = default;

	Rasterizer(VkDevice* device, VkPhysicalDevice* gpu, Allocator* allocator, Swapchain* swapchain, PipelineFeed* pipelineFeed);

	~Rasterizer();

	void createRenderPass();

	void createPipeline();

	void createDescriptorSets();

	void createFramebuffers();

	void destroyResizeResources();

	void draw(uint32_t imageIndex, vector<PerFrame>& perFrame, VkQueue* queue, vector<Mesh>& meshes);

	VkFormat findDepthFormat(const vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
};