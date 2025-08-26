#pragma once
#include "pipeline.h"

using namespace glm;

struct AccelerationStructure
{
	VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
	uint64_t deviceAddress = 0;
};

struct ShaderBindingTable
{
	uint32_t handleSize;
	uint32_t handleAlign;
	uint32_t baseAlign;

	VkBuffer sbtBuffer = VK_NULL_HANDLE;
	VkDeviceAddress sbtBase = 0;
	VkStridedDeviceAddressRegionKHR sbtRaygen{}, sbtMiss{}, sbtHit{}, sbtCallable{};
};

class RayTracer : public Pipeline
{
public:
	PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR{ nullptr };
	PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR{ nullptr };
	PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR{ nullptr };
	PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR{ nullptr };
	PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR{ nullptr };
	PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR{ nullptr };
	PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR{ nullptr };
	PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR{ nullptr };
	PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR{ nullptr };

	Queues* queues;

	VkImage storageImage = VK_NULL_HANDLE;
	VkImageView storageImageView = VK_NULL_HANDLE;
	vector<AccelerationStructure> bottomLevelASStructures;
	AccelerationStructure topLevelAS;
	ShaderBindingTable sbt;

	RayTracer() = default;

	RayTracer(VkDevice* device, VkPhysicalDevice* gpu, Allocator* allocator, Swapchain* swapchain, PipelineFeed* pipelineFeed, Queues* queues);

	~RayTracer() override;

	void createFramebuffers();

	void createDescriptorSets();

	void createPipeline();

	void handleResize();

	void createShaderBindingTable();

	void draw(uint32_t imageIndex, vector<PerFrame>& perFrame, VkQueue* queue);

	void createAccelerationStructures();

	AccelerationStructure createBottomLevelAccelerationStructure(Mesh mesh);

	AccelerationStructure createTopLevelAccelerationStructure();

	// util
	uint64_t getBufferDeviceAddress(VkBuffer buffer);

	VkTransformMatrixKHR toVkTransform(const mat4& m);
};