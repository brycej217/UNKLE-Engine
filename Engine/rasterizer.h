#pragma once
#include "pipeline.h"

class Rasterizer : public Pipeline
{
public:
	VkRenderPass renderPass{ VK_NULL_HANDLE };

	vector<UnkImage*> depthImages;
	vector<VkFramebuffer> framebuffers;

	Rasterizer() = default;

	Rasterizer(UnkDevice* device, UnkSwapchain* swapchain, DeviceResources* resources);

	~Rasterizer() override;

	void createRenderPass();

	void createPipeline();

	void createDescriptorSets();

	void createFramebuffers();

	void handleResize();

	void draw(uint32_t imageIndex);

	VkFormat findDepthFormat(const vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
};