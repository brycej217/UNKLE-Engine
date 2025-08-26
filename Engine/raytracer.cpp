#include "raytracer.h"

#include <glm/glm.hpp>
#include <array>
using namespace std;
using namespace glm;

uint64_t RayTracer::getBufferDeviceAddress(VkBuffer buffer)
{
	VkBufferDeviceAddressInfoKHR bufferDeviceAI
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = buffer
	};

	return vkGetBufferDeviceAddressKHR(*device, &bufferDeviceAI);
}

VkTransformMatrixKHR RayTracer::toVkTransform(const mat4& m)
{
	VkTransformMatrixKHR out{};

	out.matrix[0][0] = m[0][0];
	out.matrix[0][1] = m[1][0];
	out.matrix[0][2] = m[2][0];
	out.matrix[0][3] = m[3][0];

	out.matrix[1][0] = m[0][1];
	out.matrix[1][1] = m[1][1];
	out.matrix[1][2] = m[2][1];
	out.matrix[1][3] = m[3][1];

	out.matrix[2][0] = m[0][2];
	out.matrix[2][1] = m[1][2];
	out.matrix[2][2] = m[2][2];
	out.matrix[2][3] = m[3][2];
	return out;
}

void RayTracer::createFramebuffers()
{
	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

	allocator->createImage
	(
		format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		VMA_MEMORY_USAGE_AUTO,
		swapchain->dimensions.width,
		swapchain->dimensions.height,
		storageImage,
		nullptr
	);

	VkImageViewCreateInfo imageViewCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = storageImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange =
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
	VK_CHECK(vkCreateImageView(*device, &imageViewCreateInfo, nullptr, &storageImageView));

	allocator->transitionImageLayout(storageImage, format, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
}

void RayTracer::createAccelerationStructures()
{
	VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress
	{
		.deviceAddress = getBufferDeviceAddress(pipelineFeed->transformSSBO)
	};

	for (auto& mesh : pipelineFeed->meshes)
	{
		AccelerationStructure bottomLevelAS = createBottomLevelAccelerationStructure(mesh);
		bottomLevelASStructures.push_back(bottomLevelAS);
	}

	topLevelAS = createTopLevelAccelerationStructure();
}

AccelerationStructure RayTracer::createBottomLevelAccelerationStructure(Mesh mesh)
{
	VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress
	{
		.deviceAddress = getBufferDeviceAddress(pipelineFeed->vertexSSBO)
	};
	VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress
	{
		.deviceAddress = getBufferDeviceAddress(pipelineFeed->indexSSBO)
	};
	const uint32_t primitiveCount = mesh.indexCount / 3;

	// describe bottom level acceleration structure geometry
	VkAccelerationStructureGeometryKHR accelerationStructureGeometry
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
		.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
		.geometry =
		{
			.triangles =
			{
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
				.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
				.vertexData = vertexBufferDeviceAddress.deviceAddress + offsetof(Vertex, position) + VkDeviceSize(mesh.vertexOffset) * sizeof(Vertex),
				.vertexStride = sizeof(Vertex),
				.maxVertex = mesh.vertexCount - 1,
				.indexType = VK_INDEX_TYPE_UINT32,
				.indexData = indexBufferDeviceAddress.deviceAddress + VkDeviceSize(mesh.firstIndex) * sizeof(uint32_t),
				.transformData =
				{
					.deviceAddress = 0,
				},
			}
		},
		.flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
	};

	VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo
	{
		.primitiveCount = primitiveCount,
		.primitiveOffset = 0,
		.firstVertex = 0,
		.transformOffset = 0
	};

	/// BUILD ACCELERATION STRUCTURE ///

	VkAccelerationStructureBuildGeometryInfoKHR sizeInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		.geometryCount = 1,
		.pGeometries = &accelerationStructureGeometry
	};

	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
	};
	vkGetAccelerationStructureBuildSizesKHR(
		*device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&sizeInfo,
		&primitiveCount,
		&accelerationStructureBuildSizesInfo
	);

	// create scratch buffer
	VkBuffer scratchBuffer;
	allocator->createBuffer(
		accelerationStructureBuildSizesInfo.buildScratchSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		scratchBuffer,
		nullptr
	);
	VkBufferDeviceAddressInfo addressInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = scratchBuffer
	};
	VkDeviceAddress scratchAddress = getBufferDeviceAddress(scratchBuffer);

	// create bottom level acceleration structure buffer
	AccelerationStructure bottomLevelAS;
	VkBuffer blasBuffer;
	allocator->createBuffer(
		accelerationStructureBuildSizesInfo.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		blasBuffer,
		nullptr
	);
	VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
		.buffer = blasBuffer,
		.size = accelerationStructureBuildSizesInfo.accelerationStructureSize,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
	};
	vkCreateAccelerationStructureKHR(*device, &accelerationStructureCreateInfo, nullptr, &bottomLevelAS.handle);

	// create bottom level acceleration structure
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		.dstAccelerationStructure = bottomLevelAS.handle,
		.geometryCount = 1,
		.pGeometries = &accelerationStructureGeometry,
		.scratchData =
		{
			.deviceAddress = scratchAddress
		}
	};

	vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	allocator->beginTransientCommands(commandPool, commandBuffer, queues->graphicsIndex);

	vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, accelerationBuildStructureRangeInfos.data());

	allocator->endTransientCommands(commandPool, commandBuffer, queues->graphicsQueue);

	VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
		.accelerationStructure = bottomLevelAS.handle
	};
	bottomLevelAS.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(*device, &accelerationDeviceAddressInfo);
	return bottomLevelAS;
}

AccelerationStructure RayTracer::createTopLevelAccelerationStructure()
{
	vector<VkAccelerationStructureInstanceKHR> instances;

	for (int i = 0; i < pipelineFeed->meshes.size(); i++)
	{
		for (int j = 0; j < pipelineFeed->meshes[i].instanceCount; j++)
		{
			uint32_t instIndex = pipelineFeed->meshes[i].firstInstance + j;
			Instance instRec = pipelineFeed->instances[instIndex];
			mat4 matrix = pipelineFeed->transforms[instRec.transformIndex].model;

			VkAccelerationStructureInstanceKHR instance
			{
				.transform = toVkTransform(matrix),
				.instanceCustomIndex = instIndex,
				.mask = 0xFF,
				.instanceShaderBindingTableRecordOffset = 0,
				.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
				.accelerationStructureReference = bottomLevelASStructures[i].deviceAddress
			};
			instances.push_back(instance);
		}
	}

	const uint32_t instanceCount = instances.size();

	VkBuffer instanceBuffer;
	VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * instanceCount;
	allocator->createDeviceLocalBuffer
	(
		instances.data(),
		instanceBufferSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		0,
		instanceBuffer,
		nullptr
	);
	VkDeviceAddress instanceBufferDeviceAddress = getBufferDeviceAddress(instanceBuffer);

	VkAccelerationStructureGeometryKHR accelerationStructureGeometry
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
		.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
		.geometry = 
		{
			.instances = 
			{
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
				.arrayOfPointers = VK_FALSE,
				.data = 
				{
					.deviceAddress = instanceBufferDeviceAddress
				}
			}
		},
		.flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
	};

	VkAccelerationStructureBuildGeometryInfoKHR sizeInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		.geometryCount = 1,
		.pGeometries = &accelerationStructureGeometry
	};
	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
	};
	vkGetAccelerationStructureBuildSizesKHR
	(
		*device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&sizeInfo,
		&instanceCount,
		&accelerationStructureBuildSizesInfo
	);

	// create top level acceleration structure buffer
	VkBuffer tlasBuffer;
	allocator->createBuffer(
		accelerationStructureBuildSizesInfo.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		tlasBuffer,
		nullptr
	);

	AccelerationStructure topLevelAS;
	VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
		.buffer = tlasBuffer,
		.size = accelerationStructureBuildSizesInfo.accelerationStructureSize,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
	};
	vkCreateAccelerationStructureKHR(*device, &accelerationStructureCreateInfo, nullptr, &topLevelAS.handle);

	VkBuffer scratchBuffer = VK_NULL_HANDLE;
	allocator->createBuffer(
		accelerationStructureBuildSizesInfo.buildScratchSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		scratchBuffer,
		nullptr
	);
	VkBufferDeviceAddressInfo addressInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = scratchBuffer
	};
	VkDeviceAddress scratchAddress = getBufferDeviceAddress(scratchBuffer);

	VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo
	{
		.primitiveCount = instanceCount,
		.primitiveOffset = 0,
		.firstVertex = 0,
		.transformOffset = 0
	};
	vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

	// create top level acceleration structure
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		.dstAccelerationStructure = topLevelAS.handle,
		.geometryCount = 1,
		.pGeometries = &accelerationStructureGeometry,
		.scratchData =
		{
			.deviceAddress = scratchAddress
		}
	};

	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	allocator->beginTransientCommands(commandPool, commandBuffer, queues->graphicsIndex);
	vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, accelerationBuildStructureRangeInfos.data());
	allocator->endTransientCommands(commandPool, commandBuffer, queues->graphicsQueue);

	VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
		.accelerationStructure = topLevelAS.handle
	};
	topLevelAS.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(*device, &accelerationDeviceAddressInfo);
	return topLevelAS;
}

void RayTracer::createDescriptorSets()
{
	uint32_t textureCount = static_cast<uint32_t>(pipelineFeed->textureImageViews.size());
	uint32_t maxTextureCount = std::max<uint32_t>(textureCount, 1);

	// define descriptor set bindings
	VkDescriptorSetLayoutBinding instanceBinding
	{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
	};

	VkDescriptorSetLayoutBinding transformBinding
	{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
	};

	VkDescriptorSetLayoutBinding lightBinding
	{
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
	};
	VkDescriptorSetLayoutBinding accelerationStructureBinding
	{
		.binding = 3,
		.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
	};

	VkDescriptorSetLayoutBinding resultImageBinding
	{
		.binding = 4,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
	};
	VkDescriptorSetLayoutBinding cameraBinding
	{
		.binding = 5,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
	};
	VkDescriptorSetLayoutBinding vertexBinding
	{
		.binding = 6,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
	};
	VkDescriptorSetLayoutBinding indexBinding
	{
		.binding = 7,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
	};
	VkDescriptorSetLayoutBinding textureBinding
	{
		.binding = 8,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = maxTextureCount,
		.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
	};


	array<VkDescriptorSetLayoutBinding, 9> bindings = { instanceBinding, transformBinding, lightBinding, accelerationStructureBinding, resultImageBinding, cameraBinding, vertexBinding, indexBinding, textureBinding};

	const VkDescriptorBindingFlags textureBindingFlag =
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT;

	array<VkDescriptorBindingFlags, 9> flags = { 0, 0, 0, 0, 0, 0, 0, 0, textureBindingFlag};

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
		.bindingCount = bindings.size(),
		.pBindingFlags = flags.data(),
	};

	// define descriptor set layout (wrap all bindings)
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = &bindingFlags,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()
	};
	VK_CHECK(vkCreateDescriptorSetLayout(*device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout));

	// Create descriptor sets
	VkDescriptorPoolSize poolSizes[] =
	{
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
		{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxTextureCount}
	};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets = 1,
		.poolSizeCount = (uint32_t)std::size(poolSizes),
		.pPoolSizes = poolSizes,
	};
	VK_CHECK(vkCreateDescriptorPool(*device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

	uint32_t counts[] = { maxTextureCount };
	VkDescriptorSetVariableDescriptorCountAllocateInfo varCount
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
		.descriptorSetCount = 1,
		.pDescriptorCounts = counts
	};

	// allocate descriptors
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = &varCount,
		.descriptorPool = descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &descriptorSetLayout
	};
	VK_CHECK(vkAllocateDescriptorSets(*device, &descriptorSetAllocateInfo, &descrptorSet));


	// link descriptors to buffer and image handles
	vector<VkWriteDescriptorSet> writes(9);
	VkDescriptorBufferInfo instanceInfo
	{
		.buffer = pipelineFeed->instanceSSBO,
		.offset = 0,
		.range = pipelineFeed->instances.size() * sizeof(Instance)
	};
	writes[0] =
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descrptorSet,
		.dstBinding = 0,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pBufferInfo = &instanceInfo
	};

	VkDescriptorBufferInfo transformInfo
	{
		.buffer = pipelineFeed->transformSSBO,
		.offset = 0,
		.range = pipelineFeed->transforms.size() * sizeof(MVP)
	};
	writes[1] =
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descrptorSet,
		.dstBinding = 1,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pBufferInfo = &transformInfo
	};

	VkDescriptorBufferInfo lightInfo
	{
		.buffer = pipelineFeed->lightSSBO,
		.offset = 0,
		.range = pipelineFeed->lights.size() * sizeof(Light)
	};
	writes[2] =
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descrptorSet,
		.dstBinding = 2,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pBufferInfo = &lightInfo
	};
	VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
		.accelerationStructureCount = 1,
		.pAccelerationStructures = &topLevelAS.handle
	};
	writes[3] =
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = &descriptorAccelerationStructureInfo,
		.dstSet = descrptorSet,
		.dstBinding = 3,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
	};
	VkDescriptorImageInfo resultImageInfo
	{
		.imageView = storageImageView,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL,

	};
	writes[4] =
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descrptorSet,
		.dstBinding = 4,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.pImageInfo = &resultImageInfo
	};
	VkDescriptorBufferInfo cameraInfo
	{
		.buffer = pipelineFeed->cameraBuffer,
		.offset = 0,
		.range = sizeof(CameraGPU)
	};
	writes[5] =
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descrptorSet,
		.dstBinding = 5,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo = &cameraInfo
	};
	vector<VkDescriptorImageInfo> imageInfos(textureCount);
	for (int i = 0; i < textureCount; i++)
	{
		imageInfos[i] =
		{
			pipelineFeed->textureImageSampler,
			pipelineFeed->textureImageViews[i],
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
	}
	VkDescriptorBufferInfo vertexInfo
	{
		.buffer = pipelineFeed->vertexSSBO,
		.offset = 0,
		.range = pipelineFeed->vertexBufferSize
	};
	writes[6] =
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descrptorSet,
		.dstBinding = 6,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pBufferInfo = &vertexInfo
	};
	VkDescriptorBufferInfo indexInfo
	{
		.buffer = pipelineFeed->indexSSBO,
		.offset = 0,
		.range = pipelineFeed->indexBufferSize
	};
	writes[7] =
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descrptorSet,
		.dstBinding = 7,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pBufferInfo = &indexInfo
	};
	writes[8] =
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descrptorSet,
		.dstBinding = 8,
		.dstArrayElement = 0,
		.descriptorCount = textureCount,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = imageInfos.data()
	};
	vkUpdateDescriptorSets(*device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void RayTracer::createPipeline()
{
	VkPipelineLayoutCreateInfo pipelineLayoutCI
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptorSetLayout
	};
	VK_CHECK(vkCreatePipelineLayout(*device, &pipelineLayoutCI, nullptr, &pipelineLayout));

	VkShaderModule rgenModule = loadShaderModule("shaders/raygen.spv");
	VkShaderModule missModule = loadShaderModule("shaders/miss.spv");
	VkShaderModule hitModule = loadShaderModule("shaders/hit.spv");
	VkShaderModule shadowMissModule = loadShaderModule("shaders/shadow_miss.spv");


	vector<VkPipelineShaderStageCreateInfo> shaderStages;
	VkPipelineShaderStageCreateInfo rgenStage
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
		.module = rgenModule,
		.pName = "main"
	};
	shaderStages.push_back(rgenStage);

	VkPipelineShaderStageCreateInfo missStage
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_MISS_BIT_KHR,
		.module = missModule,
		.pName = "main"
	};
	shaderStages.push_back(missStage);
	
	VkPipelineShaderStageCreateInfo hitStage
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		.module = hitModule,
		.pName = "main"
	};
	shaderStages.push_back(hitStage);

	VkPipelineShaderStageCreateInfo shadowMissStage{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_MISS_BIT_KHR,
		.module = shadowMissModule,
		.pName = "main"
	};
	shaderStages.push_back(shadowMissStage);

	vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
	VkRayTracingShaderGroupCreateInfoKHR rgenGroup
	{
		.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
		.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
		.generalShader = 0,
		.closestHitShader = VK_SHADER_UNUSED_KHR,
		.anyHitShader = VK_SHADER_UNUSED_KHR,
		.intersectionShader = VK_SHADER_UNUSED_KHR,
		.pShaderGroupCaptureReplayHandle = nullptr
	};
	groups.push_back(rgenGroup);

	VkRayTracingShaderGroupCreateInfoKHR missGroup
	{
		.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
		.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
		.generalShader = 1,
		.closestHitShader = VK_SHADER_UNUSED_KHR,
		.anyHitShader = VK_SHADER_UNUSED_KHR,
		.intersectionShader = VK_SHADER_UNUSED_KHR,
		.pShaderGroupCaptureReplayHandle = nullptr
	};
	groups.push_back(missGroup);

	VkRayTracingShaderGroupCreateInfoKHR hitGroup
	{
		.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
		.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
		.generalShader = VK_SHADER_UNUSED_KHR,
		.closestHitShader = 2,
		.anyHitShader = VK_SHADER_UNUSED_KHR,
		.intersectionShader = VK_SHADER_UNUSED_KHR,
		.pShaderGroupCaptureReplayHandle = nullptr
	};
	groups.push_back(hitGroup);

	VkRayTracingShaderGroupCreateInfoKHR shadowMissGroup{
		.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
		.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
		.generalShader = 3,
		.closestHitShader = VK_SHADER_UNUSED_KHR,
		.anyHitShader = VK_SHADER_UNUSED_KHR,
		.intersectionShader = VK_SHADER_UNUSED_KHR
	};
	groups.push_back(shadowMissGroup);

	VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI
	{
		.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
		.stageCount = static_cast<uint32_t>(shaderStages.size()),
		.pStages = shaderStages.data(),
		.groupCount = static_cast<uint32_t>(groups.size()),
		.pGroups = groups.data(),
		.maxPipelineRayRecursionDepth = 2,
		.layout = pipelineLayout
	};

	VK_CHECK(vkCreateRayTracingPipelinesKHR(*device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &pipeline));

	vkDestroyShaderModule(*device, shaderStages[0].module, nullptr);
	vkDestroyShaderModule(*device, shaderStages[1].module, nullptr);
	vkDestroyShaderModule(*device, shaderStages[2].module, nullptr);
	vkDestroyShaderModule(*device, shaderStages[3].module, nullptr);
}

static inline VkDeviceSize alignUp(VkDeviceSize v, VkDeviceSize a) 
{
	return (v + a - 1) & ~(a - 1);
}

void RayTracer::createShaderBindingTable()
{
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR props
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
	};

	VkPhysicalDeviceProperties2 props2
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
		&props
	};
	vkGetPhysicalDeviceProperties2(*gpu, &props2);

	sbt.handleSize = props.shaderGroupHandleSize;
	sbt.handleAlign = props.shaderGroupHandleAlignment;
	sbt.baseAlign = props.shaderGroupBaseAlignment;

	const uint32_t groupCount = 4;
	vector<uint8_t> handles(groupCount * sbt.handleSize);
	vkGetRayTracingShaderGroupHandlesKHR(*device, pipeline, 0, groupCount, handles.size(), handles.data());

	VkDeviceSize rgenStride = alignUp(sbt.handleSize, sbt.handleAlign);
	VkDeviceSize missStride = alignUp(sbt.handleSize, sbt.handleAlign);
	VkDeviceSize hitStride = alignUp(sbt.handleSize, sbt.handleAlign);

	VkDeviceSize rgenSize = rgenStride * 1;
	VkDeviceSize missSize = missStride * 2;
	VkDeviceSize hitSize = hitStride * 1;

	VkDeviceSize rgenOffset = 0;
	VkDeviceSize missOffset = alignUp(rgenOffset + rgenSize, sbt.baseAlign);
	VkDeviceSize hitOffset = alignUp(missOffset + missSize, sbt.baseAlign);
	VkDeviceSize sbtSize = hitOffset + hitSize;

	std::vector<uint8_t> sbtBlob(sbtSize, 0);
	auto put = [&](uint32_t groupIdx, VkDeviceSize regionOffset, VkDeviceSize recIndex, VkDeviceSize stride)
		{
			std::memcpy(sbtBlob.data() + regionOffset + recIndex * stride,
				handles.data() + groupIdx * sbt.handleSize,
				sbt.handleSize);
		};
	put(0, rgenOffset, 0, rgenStride);
	put(1, missOffset, 0, missStride);
	put(3, missOffset, 1, missStride);
	put(2, hitOffset, 0, hitStride);

	allocator->createDeviceLocalBuffer(
		sbtBlob.data(),
		sbtSize,
		VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		0,
		sbt.sbtBuffer,
		nullptr);


	VkBufferDeviceAddressInfo ai{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, sbt.sbtBuffer };
	const VkDeviceAddress base = getBufferDeviceAddress(sbt.sbtBuffer);

	sbt.sbtRaygen = { base + rgenOffset, rgenStride, rgenSize };
	sbt.sbtMiss = { base + missOffset, missStride, missSize };
	sbt.sbtHit = { base + hitOffset , hitStride , hitSize };
	sbt.sbtCallable = { 0, 0, 0 };
}

void RayTracer::draw(uint32_t imageIndex, vector<PerFrame>& perFrame, VkQueue* queue)
{
	VkCommandBuffer commandBuffer = perFrame[imageIndex].commandBuffer;

	VkCommandBufferBeginInfo beginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &descrptorSet, 0, nullptr);

	vkCmdTraceRaysKHR
	(
		commandBuffer,
		&sbt.sbtRaygen,
		&sbt.sbtMiss,
		&sbt.sbtHit,
		&sbt.sbtCallable,
		swapchain->dimensions.width,
		swapchain->dimensions.height,
		1
	);

	VkImageMemoryBarrier toDst
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = perFrame[imageIndex].swapchainImage,
		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
	};
	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, nullptr, 0, nullptr, 1, &toDst);


	VkImageCopy region
	{
	  .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
	  .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
	  .extent = { swapchain->dimensions.width, swapchain->dimensions.height, 1 }
	};
	vkCmdCopyImage(commandBuffer,
		storageImage, VK_IMAGE_LAYOUT_GENERAL,
		perFrame[imageIndex].swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region);

	VkImageMemoryBarrier toPresent = toDst;
	toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toPresent.dstAccessMask = 0;
	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0, 0, nullptr, 0, nullptr, 1, &toPresent);

	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	if (perFrame[imageIndex].swapchainReleaseSemaphore == VK_NULL_HANDLE)
	{
		VkSemaphoreCreateInfo semaphoreInfo
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
		};
		VK_CHECK(vkCreateSemaphore(*device, &semaphoreInfo, nullptr, &perFrame[imageIndex].swapchainReleaseSemaphore));
	}

	VkPipelineStageFlags waitStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo info
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &perFrame[imageIndex].swapchainAcquireSemaphore,
		.pWaitDstStageMask = &waitStage,
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &perFrame[imageIndex].swapchainReleaseSemaphore
	};
	VK_CHECK(vkQueueSubmit(*queue, 1, &info, perFrame[imageIndex].queueSubmitFence));
}

void RayTracer::handleResize()
{
	if (storageImageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(*device, storageImageView, nullptr);
	}

	createFramebuffers();

	VkDescriptorImageInfo resultImageInfo
	{
		.imageView = storageImageView,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL,

	};
	VkWriteDescriptorSet resultWrite
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descrptorSet,
		.dstBinding = 4,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.pImageInfo = &resultImageInfo
	};
	vkUpdateDescriptorSets(*device, 1, &resultWrite, 0, nullptr);
}

RayTracer::RayTracer(VkDevice* device, VkPhysicalDevice* gpu, Allocator* allocator, Swapchain* swapchain, PipelineFeed* pipelineFeed, Queues* queues)
{
	this->device = device;
	this->gpu = gpu;
	this->allocator = allocator;
	this->swapchain = swapchain;
	this->pipelineFeed = pipelineFeed;
	this->queues = queues;

	vkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(*device, "vkGetBufferDeviceAddressKHR");
	vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(*device, "vkCmdBuildAccelerationStructuresKHR");
	vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(*device, "vkGetAccelerationStructureDeviceAddressKHR");
	vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(*device, "vkGetAccelerationStructureBuildSizesKHR");
	vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(*device, "vkCreateRayTracingPipelinesKHR");
	vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(*device, "vkGetRayTracingShaderGroupHandlesKHR");
	vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(*device, "vkCmdTraceRaysKHR");
	vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(*device, "vkDestroyAccelerationStructureKHR");
	vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(*device, "vkCreateAccelerationStructureKHR");


	createFramebuffers();
	createAccelerationStructures();
	createDescriptorSets();
	createPipeline();
	createShaderBindingTable();
}

RayTracer::~RayTracer()
{
	if (storageImageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(*device, storageImageView, nullptr);
		storageImageView = VK_NULL_HANDLE;
	}

	for (int i = 0; i < bottomLevelASStructures.size(); i++)
	{
		if (bottomLevelASStructures[i].handle != VK_NULL_HANDLE)
		{
			vkDestroyAccelerationStructureKHR(*device, bottomLevelASStructures[i].handle, nullptr);
		}
	}

	if (topLevelAS.handle != VK_NULL_HANDLE)
	{
		vkDestroyAccelerationStructureKHR(*device, topLevelAS.handle, nullptr);
	}
}