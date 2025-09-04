#include "raytracer.h"

#include <glm/glm.hpp>
#include <array>
using namespace std;
using namespace glm;

RayTracer::RayTracer(UnkDevice* device, UnkSwapchain* swapchain, DeviceResources* resources)
{
	this->device = device;
	this->swapchain = swapchain;
	this->resources = resources;

	createResultImage();
	createAccelerationStructures();
	createDescriptorSets();
	createPipeline();
	createShaderBindingTable();
}

void RayTracer::createResultImage()
{
	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

	resultImage = new UnkImage
	(
		device,
		swapchain->extent.width,
		swapchain->extent.height,
		format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		true
	);
	resultImage->transitionImageLayout(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
}

void RayTracer::createAccelerationStructures()
{
	VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{ getBufferDeviceAddress(resources->transformBuffer->handle) };
	VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{ getBufferDeviceAddress(resources->vertexBuffer->handle) };
	VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{ getBufferDeviceAddress(resources->indexBuffer->handle) };

	for (auto& mesh : resources->meshes)
	{
		UnkBlas* blas = new UnkBlas(device, mesh, vertexBufferDeviceAddress, indexBufferDeviceAddress);
		blasses.push_back(blas);
	}

	tlas = new UnkTlas(device, resources->meshes, resources->instances, resources->transforms, blasses);
}

void RayTracer::createDescriptorSets()
{
	uint32_t textureCount = static_cast<uint32_t>(resources->textureImages.size());
	uint32_t maxTextureCount = std::max<uint32_t>(textureCount, 1);

	vector<VkDescriptorSetLayoutBinding> bindings;
	vector<VkDescriptorBindingFlags> flags;
	
	UnkDescriptor* instanceBufferDescriptor = new UnkBufferDescriptor
	(
		resources->instanceBuffer,
		&descriptorSet,
		INSTANCE_BINDING,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		1,
		VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		0
	);
	descriptors.push_back(instanceBufferDescriptor);
	bindings.push_back(instanceBufferDescriptor->getLayoutBinding());
	flags.push_back(instanceBufferDescriptor->bindingFlags);

	UnkDescriptor* transformBufferDescriptor = new UnkBufferDescriptor
	(
		resources->transformBuffer,
		&descriptorSet,
		TRANSFORM_BINDING,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		1,
		VK_SHADER_STAGE_RAYGEN_BIT_KHR,
		0
	);
	descriptors.push_back(transformBufferDescriptor);
	bindings.push_back(transformBufferDescriptor->getLayoutBinding());
	flags.push_back(transformBufferDescriptor->bindingFlags);

	UnkDescriptor* lightBufferDescriptor = new UnkBufferDescriptor
	(
		resources->lightBuffer,
		&descriptorSet,
		LIGHT_BINDING,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		1,
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		0
	);
	descriptors.push_back(lightBufferDescriptor);
	bindings.push_back(lightBufferDescriptor->getLayoutBinding());
	flags.push_back(lightBufferDescriptor->bindingFlags);

	UnkDescriptor* asBufferDescriptor = new UnkAsDescriptor
	(
		tlas,
		&descriptorSet,
		AS_BINDING,
		VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
		1,
		VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		0
	);
	descriptors.push_back(asBufferDescriptor);
	bindings.push_back(asBufferDescriptor->getLayoutBinding());
	flags.push_back(asBufferDescriptor->bindingFlags);

	vector<UnkImage*> resultImages;
	resultImages.push_back(resultImage);
	UnkDescriptor* resultImageDescriptor = new UnkImageDescriptor
	(
		resultImages,
		&descriptorSet,
		RESULT_BINDING,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		1,
		VK_SHADER_STAGE_RAYGEN_BIT_KHR,
		0,
		VK_IMAGE_LAYOUT_GENERAL
	);
	descriptors.push_back(resultImageDescriptor);
	bindings.push_back(resultImageDescriptor->getLayoutBinding());
	flags.push_back(resultImageDescriptor->bindingFlags);

	UnkDescriptor* cameraBufferDescriptor = new UnkBufferDescriptor
	(
		resources->cameraBuffer,
		&descriptorSet,
		CAMERA_BINDING,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		1,
		VK_SHADER_STAGE_RAYGEN_BIT_KHR,
		0
	);
	descriptors.push_back(cameraBufferDescriptor);
	bindings.push_back(cameraBufferDescriptor->getLayoutBinding());
	flags.push_back(cameraBufferDescriptor->bindingFlags);

	UnkDescriptor* vertexBufferDescriptor = new UnkBufferDescriptor
	(
		resources->vertexBuffer,
		&descriptorSet,
		VERTEX_BINDING,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		1,
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		0
	);
	descriptors.push_back(vertexBufferDescriptor);
	bindings.push_back(vertexBufferDescriptor->getLayoutBinding());
	flags.push_back(vertexBufferDescriptor->bindingFlags);

	UnkDescriptor* indexBufferDescriptor = new UnkBufferDescriptor
	(
		resources->indexBuffer,
		&descriptorSet,
		INDEX_BINDING,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		1,
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		0
	);
	descriptors.push_back(indexBufferDescriptor);
	bindings.push_back(indexBufferDescriptor->getLayoutBinding());
	flags.push_back(indexBufferDescriptor->bindingFlags);

	UnkDescriptor* textureImagesDescriptor = new UnkImageDescriptor
	(
		resources->textureImages,
		&descriptorSet,
		TEXTURE_BINDING,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		textureCount,
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);
	descriptors.push_back(textureImagesDescriptor);
	bindings.push_back(textureImagesDescriptor->getLayoutBinding());
	flags.push_back(textureImagesDescriptor->bindingFlags);

	const size_t n = bindings.size();
	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
		.bindingCount = static_cast<uint32_t>(n),
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
	VK_CHECK(vkCreateDescriptorSetLayout(device->device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout));

	// Create descriptor sets
	vector<VkDescriptorPoolSize> poolSizes;
	for (int i = 0; i < descriptors.size(); i++)
	{
		VkDescriptorPoolSize poolSize
		{
			.type = descriptors[i]->descriptorType,
			.descriptorCount = descriptors[i]->count
		};
		poolSizes.push_back(poolSize);
	}

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets = 1,
		.poolSizeCount = static_cast<uint32_t>(n),
		.pPoolSizes = poolSizes.data(),
	};
	VK_CHECK(vkCreateDescriptorPool(device->device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

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
	VK_CHECK(vkAllocateDescriptorSets(device->device, &descriptorSetAllocateInfo, &descriptorSet));


	// link descriptors to buffer and image handles
	vector<VkWriteDescriptorSet> writes;
	for (int i = 0; i < descriptors.size(); i++)
	{
		writes.push_back(descriptors[i]->getDescriptorWrite());
	}
	vkUpdateDescriptorSets(device->device, writes.size(), writes.data(), 0, nullptr);
}

void RayTracer::createPipeline()
{
	VkPipelineLayoutCreateInfo pipelineLayoutCI
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptorSetLayout
	};
	VK_CHECK(vkCreatePipelineLayout(device->device, &pipelineLayoutCI, nullptr, &pipelineLayout));

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
	VK_CHECK(device->vkCreateRayTracingPipelinesKHR(device->device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &pipeline));

	vkDestroyShaderModule(device->device, shaderStages[0].module, nullptr);
	vkDestroyShaderModule(device->device, shaderStages[1].module, nullptr);
	vkDestroyShaderModule(device->device, shaderStages[2].module, nullptr);
	vkDestroyShaderModule(device->device, shaderStages[3].module, nullptr);
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
	vkGetPhysicalDeviceProperties2(device->gpu, &props2);

	sbt.handleSize = props.shaderGroupHandleSize;
	sbt.handleAlign = props.shaderGroupHandleAlignment;
	sbt.baseAlign = props.shaderGroupBaseAlignment;

	const uint32_t groupCount = 4;
	vector<uint8_t> handles(groupCount * sbt.handleSize);
	device->vkGetRayTracingShaderGroupHandlesKHR(device->device, pipeline, 0, groupCount, handles.size(), handles.data());

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

	sbt.sbtBuffer = new UnkBuffer
	(
		device,
		sbtSize,
		VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		0,
		0,
		sbtBlob.data()
	);

	VkBufferDeviceAddressInfo ai{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, sbt.sbtBuffer->handle };
	const VkDeviceAddress base = getBufferDeviceAddress(sbt.sbtBuffer->handle);

	sbt.sbtRaygen = { base + rgenOffset, rgenStride, rgenSize };
	sbt.sbtMiss = { base + missOffset, missStride, missSize };
	sbt.sbtHit = { base + hitOffset , hitStride , hitSize };
	sbt.sbtCallable = { 0, 0, 0 };
}

void RayTracer::draw(uint32_t index)
{
	UnkCommandBuffer* commandBuffer = swapchain->frames[index].commandBuffer;
	commandBuffer->beginCommand();

	VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vkCmdBindPipeline(commandBuffer->handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
	vkCmdBindDescriptorSets(commandBuffer->handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

	device->vkCmdTraceRaysKHR
	(
		commandBuffer->handle,
		&sbt.sbtRaygen,
		&sbt.sbtMiss,
		&sbt.sbtHit,
		&sbt.sbtCallable,
		swapchain->extent.width,
		swapchain->extent.height,
		1
	);

	swapchain->images[index]->transitionImageLayout(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer);

	VkImageCopy region
	{
	  .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
	  .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
	  .extent = { swapchain->extent.width, swapchain->extent.height, 1 }
	};
	vkCmdCopyImage(commandBuffer->handle,
		resultImage->handle, VK_IMAGE_LAYOUT_GENERAL,
		swapchain->images[index]->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region);

	swapchain->images[index]->transitionImageLayout(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, commandBuffer);

	commandBuffer->endCommand(false);

	if (swapchain->frames[index].swapchainReleaseSemaphore == VK_NULL_HANDLE)
	{
		VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		vkCreateSemaphore(device->device, &semaphoreInfo, nullptr, &swapchain->frames[index].swapchainReleaseSemaphore);
	}

	VkPipelineStageFlags waitStage{ VK_PIPELINE_STAGE_TRANSFER_BIT };

	VkSubmitInfo info
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &swapchain->frames[index].swapchainAcquireSemaphore,
		.pWaitDstStageMask = &waitStage,
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer->handle,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &swapchain->frames[index].swapchainReleaseSemaphore
	};
	vkQueueSubmit(device->getQueue(device->queues.graphics), 1, &info, swapchain->frames[index].queueSubmitFence);
}

void RayTracer::handleResize()
{
	if (resultImage != VK_NULL_HANDLE)
	{
		delete resultImage;
	}

	createResultImage();

	UnkImageDescriptor* descriptor = static_cast<UnkImageDescriptor*>(descriptors[RESULT_BINDING]);

	vector<UnkImage*> images;
	images.push_back(resultImage);
	descriptor->images = images;

	VkWriteDescriptorSet descriptorWrite = descriptor->getDescriptorWrite();
	vkUpdateDescriptorSets(device->device, 1, &descriptorWrite, 0, nullptr);
}

uint64_t RayTracer::getBufferDeviceAddress(VkBuffer buffer)
{
	VkBufferDeviceAddressInfoKHR bufferDeviceAI
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = buffer
	};

	return device->vkGetBufferDeviceAddressKHR(device->device, &bufferDeviceAI);
}

RayTracer::~RayTracer()
{
	if (resultImage != nullptr)
	{
		delete resultImage;
	}

	delete sbt.sbtBuffer;
	sbt.sbtBuffer = nullptr;

	for (int i = 0; i < blasses.size(); i++)
	{
		delete blasses[i];
	}

	delete tlas;
}