#include "Render/InitVulkanTypes.h"

namespace Mist
{
	namespace vkinit
	{
		VkCommandPoolCreateInfo CommandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags)
		{
			// CommandPool for the commands submited to the graphics queue.
			VkCommandPoolCreateInfo commandPoolInfo = {};
			commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			commandPoolInfo.pNext = nullptr;
			// Specify that only one command pool will submit graphics commands.
			commandPoolInfo.queueFamilyIndex = queueFamilyIndex;
			// Allow for resetting of individual command buffers.
			commandPoolInfo.flags = flags;
			return commandPoolInfo;
		}

		VkCommandBufferAllocateInfo CommandBufferCreateAllocateInfo(VkCommandPool pool, uint32_t count, VkCommandBufferLevel level)
		{
			// Allocate command buffer
			VkCommandBufferAllocateInfo commandAllocInfo = {};
			commandAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandAllocInfo.pNext = nullptr;

			// Specify command pool
			commandAllocInfo.commandPool = pool;
			// At the moment, just one command buffer...
			commandAllocInfo.commandBufferCount = count;
			// Primary command level:
			/*
			* * Primary level: those that are sent to VkQueue and do all the work.
			* * Secondary level: those that are subcommands, used to advanced multithreading scenes.
			*/
			commandAllocInfo.level = level;
			return commandAllocInfo;
		}

		VkCommandBufferBeginInfo CommandBufferBeginInfo(VkCommandBufferUsageFlags flags)
		{
			VkCommandBufferBeginInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			info.pNext = nullptr;
			info.pInheritanceInfo = nullptr;
			info.flags = flags;
			return info;
		}

		VkSubmitInfo SubmitInfo(const VkCommandBuffer* command)
		{
			VkSubmitInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			info.pNext = nullptr;
			info.waitSemaphoreCount = 0;
			info.pWaitSemaphores = nullptr;
			info.pWaitDstStageMask = nullptr;
			info.commandBufferCount = 1;
			info.pCommandBuffers = command;
			info.signalSemaphoreCount = 0;
			info.pSignalSemaphores = nullptr;
			return info;
		}

		VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags)
		{
			// Create fence
			VkFenceCreateInfo fenceCreateInfo = {};
			fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceCreateInfo.pNext = nullptr;
			fenceCreateInfo.flags = flags;
			return fenceCreateInfo;
		}

		VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags flags)
		{
			VkSemaphoreCreateInfo semaphoreCreateInfo = {};
			semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			semaphoreCreateInfo.pNext = nullptr;
			semaphoreCreateInfo.flags = flags;
			return semaphoreCreateInfo;
		}

		VkBufferMemoryBarrier BufferMemoryBarrier(VkBuffer buffer, uint32_t size, uint32_t offset, VkAccessFlags srcAccess, VkAccessFlags dstAccess, uint32_t srcQueueIndex, uint32_t dstQueueIndex)
		{
			VkBufferMemoryBarrier barrier{ .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, .pNext = nullptr };
			barrier.buffer = buffer;
			barrier.size = size;
			barrier.offset = offset;
			barrier.srcQueueFamilyIndex = srcQueueIndex;
			barrier.dstQueueFamilyIndex = dstQueueIndex;
			barrier.srcAccessMask = srcAccess;
			barrier.dstAccessMask = dstAccess;
			return barrier;
		}

		VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule, const char* entryPoint)
		{
			VkPipelineShaderStageCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			info.pName = nullptr;
			info.stage = stage;
			info.module = shaderModule;
			info.pName = entryPoint && *entryPoint ? entryPoint:"main";
			return info;
		}

		VkPipelineVertexInputStateCreateInfo PipelineVertexInputStageCreateInfo()
		{
			// Empty right now...
			VkPipelineVertexInputStateCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			info.pNext = nullptr;
			info.vertexBindingDescriptionCount = 0;
			info.vertexAttributeDescriptionCount = 0;
			return info;
		}

		VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyCreateInfo(VkPrimitiveTopology topology)
		{
			VkPipelineInputAssemblyStateCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			info.pNext = nullptr;
			info.topology = topology;
			info.primitiveRestartEnable = VK_FALSE;
			return info;
		}

		VkPipelineRasterizationStateCreateInfo PipelineRasterizationStateCreateInfo(VkPolygonMode polygonMode)
		{
			VkPipelineRasterizationStateCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			info.pNext = nullptr;
			info.depthClampEnable = VK_FALSE;
			// Don't discard primitives before rasterization. 
			// rasterizedDiscardEnable will discard the primitives before rasterization. Useful for postprocessing vertex stages.
			info.rasterizerDiscardEnable = VK_FALSE;
			info.polygonMode = polygonMode;
			info.lineWidth = 1.f;
			// No backface culling
			//info.cullMode = VK_CULL_MODE_NONE;
			info.cullMode = VK_CULL_MODE_FRONT_BIT;
			info.frontFace = VK_FRONT_FACE_CLOCKWISE;
			// No depth bias
			info.depthBiasEnable = VK_FALSE;
			info.depthBiasConstantFactor = 0.f;
			info.depthBiasClamp = 0.f;
			info.depthBiasSlopeFactor = 0.f;

			return info;
		}

		VkPipelineMultisampleStateCreateInfo PipelineMultisampleStateCreateInfo()
		{
			VkPipelineMultisampleStateCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			info.pNext = nullptr;
			info.sampleShadingEnable = VK_FALSE;
			// No multisample as default, just 1 sample per pixel.
			info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			info.minSampleShading = 1.f;
			info.pSampleMask = nullptr;
			info.alphaToCoverageEnable = VK_FALSE;
			info.alphaToOneEnable = VK_FALSE;
			return info;
		}

		VkPipelineColorBlendAttachmentState PipelineColorBlendAttachmentState()
		{
			VkPipelineColorBlendAttachmentState blendAttachment = {};
			blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
				| VK_COLOR_COMPONENT_G_BIT
				| VK_COLOR_COMPONENT_B_BIT
				| VK_COLOR_COMPONENT_A_BIT;
			blendAttachment.blendEnable = VK_FALSE;
			return blendAttachment;
		}

		VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo()
		{
			VkPipelineLayoutCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			info.pNext = nullptr;

			info.flags = 0;
			info.setLayoutCount = 0;
			info.pSetLayouts = nullptr;
			info.pushConstantRangeCount = 0;
			info.pPushConstantRanges = nullptr;
			return info;
		}

		VkPipelineDepthStencilStateCreateInfo PipelineDepthStencilCreateInfo(bool depthTest, bool depthWrite, VkCompareOp operation)
		{
			VkPipelineDepthStencilStateCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			info.pNext = nullptr;
			info.depthTestEnable = depthTest ? VK_TRUE : VK_FALSE;
			info.depthWriteEnable = depthWrite ? VK_TRUE : VK_FALSE;
			info.depthCompareOp = depthTest ? operation : VK_COMPARE_OP_ALWAYS;
			info.depthBoundsTestEnable = VK_FALSE;
			info.minDepthBounds = 0.f; // Optional
			info.maxDepthBounds = 1.f; // Optional
			info.stencilTestEnable = VK_FALSE;
			return info;
		}

		VkAttachmentDescription RenderPassAttachmentDescription(EFormat format, EImageLayout finalLayout)
		{
			VkAttachmentDescription attachmentDesc;
			attachmentDesc.flags = 0;
			attachmentDesc.format = tovk::GetFormat(format);
			attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;// VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			attachmentDesc.finalLayout = tovk::GetImageLayout(finalLayout);
			return attachmentDesc;
		}

		VkRenderPassBeginInfo RenderPassBeginInfo(VkRenderPass renderPass, VkFramebuffer framebuffer, VkRect2D area, const VkClearValue* clearArray, uint32_t clearCount)
		{
			VkRenderPassBeginInfo info;
			info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			info.pNext = nullptr;
			info.clearValueCount = clearCount;
			info.pClearValues = clearArray;
			info.framebuffer = framebuffer;
			info.renderArea = area;
			info.renderPass = renderPass;
			return info;
		}

		VkFramebufferCreateInfo FramebufferCreateInfo(VkRenderPass pass, uint32_t width, uint32_t height, const VkImageView* viewArray, uint32_t viewCount, uint32_t flags, uint32_t layers)
		{
			VkFramebufferCreateInfo info;
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.pNext = nullptr;
			info.width = width;
			info.height = height;
			info.flags = flags;
			info.layers = layers;
			info.renderPass = pass;
			info.attachmentCount = viewCount;
			info.pAttachments = viewArray;
			return info;
		}

		VkImageCreateInfo ImageCreateInfo(EFormat format, EImageUsage usageFlags, tExtent3D extent, uint32_t arrayLayers, uint32_t mipLevels, VkImageCreateFlags flags)
		{
			VkImageCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			info.pNext = nullptr;
			info.imageType = VK_IMAGE_TYPE_2D;
			info.format = tovk::GetFormat(format);
			info.extent = extent;
			info.mipLevels = mipLevels;
			info.arrayLayers = arrayLayers;
			info.samples = VK_SAMPLE_COUNT_1_BIT;
			info.tiling = VK_IMAGE_TILING_OPTIMAL;
			info.usage = tovk::GetImageUsage(usageFlags);
			info.flags = flags;
			return info;
		}

		VkImageViewCreateInfo ImageViewCreateInfo(EFormat format, VkImage image, EImageAspect aspectFlags, uint32_t baseArrayLayer, uint32_t layerCount)
		{
			VkImageViewCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.pNext = nullptr;
			info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			info.image = image;
			info.format = tovk::GetFormat(format);
			info.subresourceRange.baseMipLevel = 0;
			info.subresourceRange.levelCount = 1;
			info.subresourceRange.baseArrayLayer = baseArrayLayer;
			info.subresourceRange.layerCount = layerCount;
			info.subresourceRange.aspectMask = tovk::GetImageUsage(aspectFlags);
			return info;
		}

		VkSamplerCreateInfo SamplerCreateInfo(EFilterType filters, ESamplerAddressMode samplerAddressMode)
		{
			VkSamplerCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			info.pNext = nullptr;
			info.magFilter = tovk::GetFilter(filters);
			info.minFilter = tovk::GetFilter(filters);
			info.addressModeU = tovk::GetSamplerAddressMode(samplerAddressMode);
			info.addressModeV = tovk::GetSamplerAddressMode(samplerAddressMode);
			info.addressModeW = tovk::GetSamplerAddressMode(samplerAddressMode);
			return info;
		}

		VkWriteDescriptorSet ImageWriteDescriptor(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding)
		{
			VkWriteDescriptorSet write = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.pNext = nullptr;
			write.dstBinding = binding;
			write.dstSet = dstSet;
			write.descriptorCount = 1;
			write.descriptorType = type;
			write.pImageInfo = imageInfo;
			return write;
		}

		VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(const VkDescriptorSetLayoutBinding* layoutBindings, uint32_t layoutBindingsCount, VkDescriptorSetLayoutCreateFlags flags)
		{
			VkDescriptorSetLayoutCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			info.pNext = nullptr;
			info.flags = flags;
			info.bindingCount = layoutBindingsCount;
			info.pBindings = layoutBindings;
			return info;
		}

		VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo(VkDescriptorPool descriptorPool, const VkDescriptorSetLayout* layouts, uint32_t poolCount)
		{
			VkDescriptorSetAllocateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			info.pNext = nullptr;
			info.descriptorPool = descriptorPool;
			info.descriptorSetCount = poolCount;
			info.pSetLayouts = layouts;
			return info;
		}

		VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding)
		{
			VkDescriptorSetLayoutBinding layoutBinding = {};
			layoutBinding.binding = binding;
			layoutBinding.descriptorCount = 1;
			layoutBinding.descriptorType = type;
			layoutBinding.pImmutableSamplers = nullptr;
			layoutBinding.stageFlags = stageFlags;
			return layoutBinding;
		}

		VkWriteDescriptorSet DescriptorSetWriteBuffer(VkDescriptorType type, VkDescriptorSet destSet, const VkDescriptorBufferInfo* bufferInfo, uint32_t binding)
		{
			VkWriteDescriptorSet write = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.pNext = nullptr;
			write.dstBinding = binding;
			write.dstSet = destSet;
			write.descriptorCount = 1;
			write.descriptorType = type;
			write.pBufferInfo = bufferInfo;
			return write;
		}
	}
}