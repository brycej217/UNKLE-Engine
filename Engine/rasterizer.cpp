#include "rasterizer.h"
#include <array>

using namespace std;

Rasterizer::Rasterizer(UnkDevice* device, UnkSwapchain* swapchain, DeviceResources* resources)
{
	this->device = device;
	this->swapchain = swapchain;
	this->resources = resources;

	createDescriptorSets(); // must be created before pipeline creation (pipeline layout)

	createRenderPass();
	createPipeline();
	createFramebuffers();
}

void Rasterizer::createDescriptorSets()
{
	enum
	{
		INSTANCE_BINDING,
		TRANSFORM_BINDING,
		LIGHT_BINDING,
		TEXTURE_BINDING
	};

	uint32_t rawTextureCount = static_cast<uint32_t>(resources->textureImages.size());
	uint32_t textureCount = std::max<uint32_t>(rawTextureCount, 1);

	vector<VkDescriptorSetLayoutBinding> bindings;
	vector<VkDescriptorBindingFlags> flags;

	UnkDescriptor* instanceBufferDescriptor = new UnkBufferDescriptor
	(
		resources->instanceBuffer,
		&descriptorSet,
		INSTANCE_BINDING,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		1,
		VK_SHADER_STAGE_VERTEX_BIT,
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
		VK_SHADER_STAGE_VERTEX_BIT,
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
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0
	);
	descriptors.push_back(lightBufferDescriptor);
	bindings.push_back(lightBufferDescriptor->getLayoutBinding());
	flags.push_back(lightBufferDescriptor->bindingFlags);

	UnkDescriptor* textureImagesDescriptor = new UnkImageDescriptor
	(
		resources->textureImages,
		&descriptorSet,
		TEXTURE_BINDING,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		textureCount,
		VK_SHADER_STAGE_FRAGMENT_BIT,
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

	uint32_t counts[] = { textureCount };
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

void Rasterizer::createRenderPass()
{
	VkAttachmentDescription colorAttachment
	{
		.format = swapchain->surfaceFormat.format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};
	VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

	VkAttachmentDescription depthAttachment
	{
		.format = findDepthFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT),
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};
	VkAttachmentReference depthRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpass
	{
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorRef,
		.pDepthStencilAttachment = &depthRef
	};

	VkSubpassDependency dependency
	{
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
	};

	array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
	VkRenderPassCreateInfo renderPassInfo
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments = attachments.data(),
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency
	};

	VK_CHECK(vkCreateRenderPass(device->device, &renderPassInfo, nullptr, &renderPass));
}

void Rasterizer::createPipeline()
{
	// define vertex inputs
	VkPipelineInputAssemblyStateCreateInfo inputAssembly
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};

	VkVertexInputBindingDescription binding
	{
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};

	array<VkVertexInputAttributeDescription, 3> attributeDescriptions
	{
		{{.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, position)},
		{.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal)},
		{.location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, texCoord)}},
	};

	VkPipelineVertexInputStateCreateInfo vertexInput
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &binding,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
		.pVertexAttributeDescriptions = attributeDescriptions.data()
	};

	// define pipeline layout (using descriptor set layout)
	VkPipelineLayoutCreateInfo pipelineLayoutInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptorSetLayout
	};
	VK_CHECK(vkCreatePipelineLayout(device->device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

	// define basic settings
	VkPipelineRasterizationStateCreateInfo rasterizer
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.cullMode = VK_CULL_MODE_NONE,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f,
	};

	VkPipelineColorBlendAttachmentState blendAttachment
	{
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	VkPipelineColorBlendStateCreateInfo blend
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &blendAttachment
	};

	VkPipelineViewportStateCreateInfo viewport
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};

	VkPipelineDepthStencilStateCreateInfo depthStencil
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f
	};

	VkPipelineMultisampleStateCreateInfo multisample
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};

	// define dynamic states
	array<VkDynamicState, 2> dynamics{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamic
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<uint32_t>(dynamics.size()),
		.pDynamicStates = dynamics.data()
	};

	array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

	shaderStages[0] =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = loadShaderModule("shaders/vert.spv"),
		.pName = "main"
	};

	shaderStages[1] =
	{
	.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
	.module = loadShaderModule("shaders/frag.spv"),
	.pName = "main"
	};

	VkGraphicsPipelineCreateInfo pipelineCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = static_cast<uint32_t>(shaderStages.size()),
		.pStages = shaderStages.data(),
		.pVertexInputState = &vertexInput,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewport,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisample,
		.pDepthStencilState = &depthStencil,
		.pColorBlendState = &blend,
		.pDynamicState = &dynamic,
		.layout = pipelineLayout,
		.renderPass = renderPass
	};

	VK_CHECK(vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline));

	vkDestroyShaderModule(device->device, shaderStages[0].module, nullptr);
	vkDestroyShaderModule(device->device, shaderStages[1].module, nullptr);
}

void Rasterizer::createFramebuffers()
{
	VkFormat depthFormat = findDepthFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	framebuffers.clear();

	for (auto& image : swapchain->images)
	{
		UnkImage* depthImage = new UnkImage
		(
			device,
			swapchain->extent.width,
			swapchain->extent.height,
			depthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			true,
			VK_IMAGE_ASPECT_DEPTH_BIT
		);
		depthImage->transitionImageLayout(VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		depthImages.push_back(depthImage);

		array<VkImageView, 2> attachments =
		{
			image->view,
			depthImage->view
		};

		VkFramebufferCreateInfo framebufferInfo
		{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = renderPass,
			.attachmentCount = static_cast<uint32_t>(attachments.size()),
			.pAttachments = attachments.data(),
			.width = swapchain->extent.width,
			.height = swapchain->extent.height,
			.layers = 1
		};

		VkFramebuffer framebuffer;
		vkCreateFramebuffer(device->device, &framebufferInfo, nullptr, &framebuffer);
		framebuffers.push_back(framebuffer);
	}
}

/*
* Destroys:
* - Depth Images
* - Depth Image Views
* - Framebuffers
*/
void Rasterizer::handleResize()
{
	for (auto depthImage : depthImages)
	{
		delete depthImage;
	}
	depthImages.clear();

	for (auto& framebuffer : framebuffers)
	{
		vkDestroyFramebuffer(device->device, framebuffer, nullptr);
		framebuffer = VK_NULL_HANDLE;
	}

	createFramebuffers();
}

void Rasterizer::draw(uint32_t index)
{
	VkFramebuffer framebuffer = framebuffers[index];
	UnkCommandBuffer* commandBuffer = swapchain->frames[index].commandBuffer;
	commandBuffer->beginCommand();


	array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBegin
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = renderPass,
		.framebuffer = framebuffer,
		.renderArea =
		{
			.extent =
			{
				.width = swapchain->extent.width,
				.height = swapchain->extent.height
			}
		},
		.clearValueCount = static_cast<uint32_t>(clearValues.size()),
		.pClearValues = clearValues.data()
	};

	vkCmdBeginRenderPass(commandBuffer->handle, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(commandBuffer->handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	VkViewport viewport
	{
		.width = static_cast<float>(swapchain->extent.width),
		.height = static_cast<float>(swapchain->extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};
	vkCmdSetViewport(commandBuffer->handle, 0, 1, &viewport);

	VkRect2D scissor
	{
		.extent =
		{
			.width = swapchain->extent.width,
			.height = swapchain->extent.height
		}
	};
	vkCmdSetScissor(commandBuffer->handle, 0, 1, &scissor);

	VkDeviceSize offset = 0;

	vkCmdBindVertexBuffers(commandBuffer->handle, 0, 1, &resources->vertexBuffer->handle, &offset);
	vkCmdBindIndexBuffer(commandBuffer->handle, resources->indexBuffer->handle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdBindDescriptorSets(commandBuffer->handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

	vkCmdDrawIndexedIndirect
	(
		commandBuffer->handle,
		resources->drawCommandBuffer->handle, 
		0, 
		static_cast<uint32_t>(resources->drawCommandBuffer->size / sizeof(VkDrawIndexedIndirectCommand)), 
		sizeof(VkDrawIndexedIndirectCommand)
	);

	vkCmdEndRenderPass(commandBuffer->handle);

	commandBuffer->endCommand(false);

	if (swapchain->frames[index].swapchainReleaseSemaphore == VK_NULL_HANDLE)
	{
		VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		vkCreateSemaphore(device->device, &semaphoreInfo, nullptr, &swapchain->frames[index].swapchainReleaseSemaphore);
	}

	VkPipelineStageFlags waitStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

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

VkFormat Rasterizer::findDepthFormat(const vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(device->gpu, format, &properties);
		if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}

	throw runtime_error("failed to find supported depth format");
}

Rasterizer::~Rasterizer()
{
	for (auto& framebuffer : framebuffers)
	{
		vkDestroyFramebuffer(device->device, framebuffer, nullptr);
	}

	for (auto depthImage : depthImages)
	{
		delete depthImage;
	}
	depthImages.clear();

	if (renderPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(device->device, renderPass, nullptr);
	}
}