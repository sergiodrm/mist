#pragma once

#include <vulkan/vulkan.h>
#include "RenderTypes.h"

namespace Mist
{
	namespace vkinit
	{
		/**
	 * Commands builders
	 */
		VkCommandPoolCreateInfo CommandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
		VkCommandBufferAllocateInfo CommandBufferCreateAllocateInfo(VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		/**
		 * Command utils
		 */
		VkCommandBufferBeginInfo CommandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);
		VkSubmitInfo SubmitInfo(const VkCommandBuffer* command);

		/**
		 * Sync vulkan types builders
		 */
		VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags = 0);
		VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);

		/**
		 * Pipeline related types builders.
		 */
		 // Build information to create a single shader stage for a pipeline.
		VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule);
		// Build information about vertex buffers and vertex formats, data layout, etc...
		VkPipelineVertexInputStateCreateInfo PipelineVertexInputStageCreateInfo();
		// Build information about what type of topology will be used in the pipeline. (triangles, lines, points...)
		// Topology types:
		// * VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		// * VK_PRIMITIVE_TOPOLOGY_POINT_LIST
		// * VK_PRIMITIVE_TOPOLOGY_LINE_LIST
		VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyCreateInfo(VkPrimitiveTopology topology);
		// Build configuration for the fixed function rasterization. Set backface culling, lines width or wireframe.
		VkPipelineRasterizationStateCreateInfo PipelineRasterizationStateCreateInfo(VkPolygonMode polygonMode);
		// Build configuration for MSAA
		VkPipelineMultisampleStateCreateInfo PipelineMultisampleStateCreateInfo();
		// Build color attachment state for specifing how the pipeline blends.
		VkPipelineColorBlendAttachmentState PipelineColorBlendAttachmentState();
		// Build pipeline layout.
		VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo();
		// Build depth stencil state for pipeline
		VkPipelineDepthStencilStateCreateInfo PipelineDepthStencilCreateInfo(bool depthTest, bool depthWrite, VkCompareOp operation);

		/**
		 * Render pass types
		 */
		VkAttachmentDescription RenderPassAttachmentDescription(EFormat format, EImageLayout finalLayout);
		VkRenderPassBeginInfo RenderPassBeginInfo(VkRenderPass renderPass, VkFramebuffer framebuffer, VkRect2D area, const VkClearValue* clearArray, uint32_t clearCount);

		/**
		 * Framebuffer
		 */
		VkFramebufferCreateInfo FramebufferCreateInfo(VkRenderPass pass, uint32_t width, uint32_t height, const VkImageView* viewArray, uint32_t viewCount, uint32_t flags = 0, uint32_t layers = 1);

		/**
		 * Image types builders
		 */
		VkImageCreateInfo ImageCreateInfo(EFormat format, EImageUsage usageFlags, tExtent3D extent, uint32_t arrayLayers = 1);
		VkImageViewCreateInfo ImageViewCreateInfo(EFormat format, VkImage image, EImageAspect aspectFlags, uint32_t baseArrayLayer = 0, uint32_t layerCount = 1);
		VkSamplerCreateInfo SamplerCreateInfo(EFilterType filters, ESamplerAddressMode samplerAddressMode = SAMPLER_ADDRESS_MODE_REPEAT);
		VkWriteDescriptorSet ImageWriteDescriptor(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding);

		/**
		 * Descriptor Sets
		 */
		VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(const VkDescriptorSetLayoutBinding* layoutBindings, uint32_t layoutBindingsCount, VkDescriptorSetLayoutCreateFlags flags = 0);
		VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo(VkDescriptorPool descriptorPool, const VkDescriptorSetLayout* layouts, uint32_t poolCount);
		VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);
		VkWriteDescriptorSet DescriptorSetWriteBuffer(VkDescriptorType type, VkDescriptorSet destSet, const VkDescriptorBufferInfo* bufferInfo, uint32_t binding);

	}

}