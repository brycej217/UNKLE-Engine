#pragma once
#include "pipeline.h"

#include "unk_blas.h"
#include "unk_tlas.h"
#include "unk_as_descriptor.h"

using namespace glm;

// define descriptor set bindings
enum
{
	INSTANCE_BINDING,
	TRANSFORM_BINDING,
	LIGHT_BINDING,
	AS_BINDING,
	RESULT_BINDING,
	CAMERA_BINDING,
	VERTEX_BINDING,
	INDEX_BINDING,
	TEXTURE_BINDING
};

struct ShaderBindingTable
{
	uint32_t handleSize;
	uint32_t handleAlign;
	uint32_t baseAlign;

	UnkBuffer* sbtBuffer;
	VkDeviceAddress sbtBase = 0;
	VkStridedDeviceAddressRegionKHR sbtRaygen{}, sbtMiss{}, sbtHit{}, sbtCallable{};
};

class RayTracer : public Pipeline
{
public:
	UnkImage* resultImage;

	vector<UnkBlas*> blasses;
	UnkTlas* tlas;

	ShaderBindingTable sbt;

	RayTracer() = default;

	RayTracer(UnkDevice* device, UnkSwapchain* swapchain, DeviceResources* resources);

	~RayTracer() override;

	void createResultImage();

	void createDescriptorSets();

	void createPipeline();

	void handleResize();

	void createShaderBindingTable();

	void draw(uint32_t imageIndex);

	void createAccelerationStructures();

	// util
	uint64_t getBufferDeviceAddress(VkBuffer buffer);

	VkTransformMatrixKHR toVkTransform(const mat4& m);
};