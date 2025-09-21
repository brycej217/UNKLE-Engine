#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <vulkan/vulkan.h>
#include <iostream>
#include <set>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "unk_buffer.h"
#include "unk_image.h"

using namespace glm;
using namespace std;

struct Instance
{
	uint32_t transformIndex;
	uint32_t textureIndex;
	uint32_t baseIndex;
	uint32_t baseVertex;
};

struct Mesh
{
	int32_t vertexOffset = 0; // first vertex corrseponding to this mesh
	uint32_t vertexCount = 0;

	uint32_t indexCount = 0;
	uint32_t firstIndex = 0;

	uint32_t instanceCount = 0;
	uint32_t firstInstance = -1;

	VkDrawIndexedIndirectCommand getDrawCommand()
	{
		VkDrawIndexedIndirectCommand drawCommand
		{
			.indexCount = this->indexCount,
			.instanceCount = this->instanceCount,
			.firstIndex = this->firstIndex,
			.vertexOffset = this->vertexOffset,
			.firstInstance = this->firstInstance
		};

		return drawCommand;
	}
};

struct PushConstants
{
	uint32_t pointLightCount;
	uint32_t dirLightCount;
	float _pad0[2];
};

struct DirectionalLight
{
	vec3 direction; float _pad0;
	vec3 color; float _pad1;
};

struct PointLight
{
	vec3 position; float _pad0;
	vec3 direction; float _pad1;
	vec3 color; float _pad2;

	float constant;
	float linear;
	float quadratic;
	float _pad3;
};

struct MVP
{
	mat4 model;
	mat4 view;
	mat4 proj;
};

struct Vertex
{
	vec3 position; float _pad0;
	vec3 normal; float _pad1;
	vec2 texCoord; vec2 _pad2;

	bool operator==(const Vertex& other) const
	{
		return position == other.position && normal == other.normal && texCoord == other.texCoord;
	}
};

struct DeviceResources
{
	vector<Mesh> meshes;

	UnkBuffer* vertexBuffer;

	UnkBuffer* indexBuffer;

	vector<Instance> instances;
	UnkBuffer* instanceBuffer;

	vector<MVP> transforms;
	UnkBuffer* transformBuffer;

	vector<PointLight> pointLights;
	vector<DirectionalLight> dirLights;
	UnkBuffer* pointLightBuffer;
	UnkBuffer* dirLightBuffer;

	UnkBuffer* cameraBuffer;

	VkSampler sampler;
	vector<UnkImage*> textureImages;

	UnkBuffer* drawCommandBuffer;

	void destroy(UnkDevice* device)
	{
		delete vertexBuffer;
		delete indexBuffer;
		delete instanceBuffer;
		delete transformBuffer;
		delete pointLightBuffer;
		delete dirLightBuffer;
		delete cameraBuffer;
		delete drawCommandBuffer;

		if (sampler != VK_NULL_HANDLE)
		{
			vkDestroySampler(device->device, sampler, nullptr);
		}

		for (auto& image : textureImages)
		{
			delete image;
		}
	}
};

struct Transform
{
	vec3 position = vec3(0.0f);
	quat rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);

	mat4 getWorldMatrix() const
	{
		return translate(mat4(1.0f), position) * mat4_cast(rotation);
	}

	vec3 getForward() const { return rotation * vec3(0.0f, 0.0f, -1.0f); }
	vec3 getUp() const { return rotation * vec3(0.0f, 1.0f, 0.0f); }
	vec3 getRight() const { return rotation * vec3(1.0f, 0.0f, 0.0f); }
};

struct CameraGPU
{
	mat4 viewInv;
	mat4 projInv;
};

struct Camera
{
	Transform transform;

	mat4 getViewMatrix() const
	{
		return inverse(transform.getWorldMatrix());
	}

	vec3 getForward() const { return transform.getForward(); }
	vec3 getUp() const { return transform.getUp(); }
	vec3 getRight() const { return transform.getRight(); }
};