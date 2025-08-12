#include "rasterizer.h"
#include <array>

using namespace std;

void Rasterizer::createRenderPass()
{
	LOG("Initializing render pass");

	VkAttachmentDescription colorAttachment
	{
		.format = swapchain->dimensions.format,
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

	VK_CHECK(vkCreateRenderPass(*device, &renderPassInfo, nullptr, &renderPass));
}

void Rasterizer::createPipeline()
{
	LOG("Initializing pipeline");

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
	VK_CHECK(vkCreatePipelineLayout(*device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

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

	VK_CHECK(vkCreateGraphicsPipelines(*device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline));

	vkDestroyShaderModule(*device, shaderStages[0].module, nullptr);
	vkDestroyShaderModule(*device, shaderStages[1].module, nullptr);
}

void Rasterizer::createDescriptorSets()
{
	LOG("Initializing descriptor sets");

	uint32_t textureCount = static_cast<uint32_t>(pipelineFeed->textureImageViews.size());
	uint32_t maxTextureCount = std::max<uint32_t>(textureCount, 1);
	
	cout << "texture count: " << textureCount << endl;

	// define descriptor set bindings
	VkDescriptorSetLayoutBinding instanceBinding
	{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
	};

	VkDescriptorSetLayoutBinding transformBinding
	{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
	};

	VkDescriptorSetLayoutBinding lightBinding
	{
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
	};

	VkDescriptorSetLayoutBinding textureBinding
	{
		.binding = 3,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = maxTextureCount,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
	};

	array<VkDescriptorSetLayoutBinding, 4> bindings = { instanceBinding, transformBinding, lightBinding, textureBinding };

	const VkDescriptorBindingFlags textureBindingFlag =
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT;

	array<VkDescriptorBindingFlags, 4> flags = { 0, 0, 0, textureBindingFlag };

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
	vector<VkWriteDescriptorSet> writes(4);
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
	writes[3] =
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descrptorSet,
		.dstBinding = 3,
		.dstArrayElement = 0,
		.descriptorCount = textureCount,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = imageInfos.data()
	};


	vkUpdateDescriptorSets(*device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void Rasterizer::createFramebuffers()
{
	LOG("Initializing framebuffers");

	VkFormat depthFormat = findDepthFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	swapchainFramebuffers.clear();
	for (auto& imageView : swapchain->imageViews)
	{
		VkImage depthImage;
		VkImageView depthImageView;
		// create depth image
		allocator->createImage(depthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY,
			swapchain->dimensions.width,
			swapchain->dimensions.height,
			depthImage,
			nullptr);
		allocator->transitionImageLayout(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

		VkImageViewCreateInfo viewInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = depthImage,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = depthFormat,
			.subresourceRange =
			{
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
		};
		VK_CHECK(vkCreateImageView(*device, &viewInfo, nullptr, &depthImageView));
		depthImages.push_back(depthImage);

		array<VkImageView, 2> attachments =
		{
			imageView,
			depthImageView
		};

		VkFramebufferCreateInfo framebufferInfo
		{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = renderPass,
			.attachmentCount = static_cast<uint32_t>(attachments.size()),
			.pAttachments = attachments.data(),
			.width = swapchain->dimensions.width,
			.height = swapchain->dimensions.height,
			.layers = 1
		};

		VkFramebuffer framebuffer;
		VK_CHECK(vkCreateFramebuffer(*device, &framebufferInfo, nullptr, &framebuffer));
		swapchainFramebuffers.push_back(framebuffer);
		depthImageViews.push_back(depthImageView);
	}
}

/*
* Destroys:
* - Depth Images
* - Depth Image Views
* - Framebuffers
*/
void Rasterizer::destroyResizeResources()
{
	for (auto depthImage : depthImages)
	{
		allocator->destroyImage(depthImage);
	}
	depthImages.clear();

	for (auto& depthImageView : depthImageViews)
	{
		vkDestroyImageView(*device, depthImageView, nullptr);
		depthImageView = VK_NULL_HANDLE;
	}

	for (auto& framebuffer : swapchainFramebuffers)
	{
		vkDestroyFramebuffer(*device, framebuffer, nullptr);
		framebuffer = VK_NULL_HANDLE;
	}
}

void Rasterizer::draw(uint32_t imageIndex, vector<PerFrame>& perFrame, VkQueue* queue, vector<Mesh>& meshes)
{
	VkFramebuffer framebuffer = swapchainFramebuffers[imageIndex];
	VkCommandBuffer commandBuffer = perFrame[imageIndex].commandBuffer;

	VkCommandBufferBeginInfo beginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

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
				.width = swapchain->dimensions.width,
				.height = swapchain->dimensions.height
			}
		},
		.clearValueCount = static_cast<uint32_t>(clearValues.size()),
		.pClearValues = clearValues.data()
	};

	vkCmdBeginRenderPass(commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	VkViewport viewport
	{
		.width = static_cast<float>(swapchain->dimensions.width),
		.height = static_cast<float>(swapchain->dimensions.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor
	{
		.extent =
		{
			.width = swapchain->dimensions.width,
			.height = swapchain->dimensions.height
		}
	};
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	VkDeviceSize offset = 0;

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &pipelineFeed->vertexSSBO, &offset);
	vkCmdBindIndexBuffer(commandBuffer, pipelineFeed->indexSSBO, 0, VK_INDEX_TYPE_UINT32);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descrptorSet, 0, nullptr);

	vkCmdDrawIndexedIndirect(commandBuffer, pipelineFeed->drawCommandBuffer, 0, static_cast<uint32_t>(pipelineFeed->drawCommands.size()), sizeof(VkDrawIndexedIndirectCommand));

	vkCmdEndRenderPass(commandBuffer);

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

VkFormat Rasterizer::findDepthFormat(const vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(*gpu, format, &properties);
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

Rasterizer::Rasterizer(VkDevice* device, VkPhysicalDevice* gpu, Allocator* allocator, Swapchain* swapchain, PipelineFeed* pipelineFeed)
{
	this->device = device;
	this->gpu = gpu;
	this->allocator = allocator;
	this->swapchain = swapchain;
	this->pipelineFeed = pipelineFeed;
	
	createDescriptorSets(); // must be created before pipeline creation (pipeline layout)

	createRenderPass();
	createPipeline();
	createFramebuffers();
}

Rasterizer::~Rasterizer()
{
	for (auto depthImage : depthImages)
	{
		allocator->destroyImage(depthImage);
	}
	depthImages.clear();

	for (auto& depthImageView : depthImageViews)
	{
		vkDestroyImageView(*device, depthImageView, nullptr);
		depthImageView = VK_NULL_HANDLE;
	}
}