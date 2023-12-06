#pragma once

#include <RenderTypes.h>

namespace vkmmc
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
	VkSubmitInfo SubmitInfo(VkCommandBuffer* command);

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
	 * Image types builders
	 */
	VkImageCreateInfo ImageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
	VkImageViewCreateInfo ImageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);
	VkSamplerCreateInfo SamplerCreateInfo(VkFilter filters, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
	VkWriteDescriptorSet ImageWriteDescriptor(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding);

	/**
	 * Descriptor Sets
	 */
	VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(VkDescriptorSetLayoutBinding* layoutBindings, uint32_t layoutBindingsCount, VkDescriptorSetLayoutCreateFlags flags = 0);
	VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo(VkDescriptorPool descriptorPool, const VkDescriptorSetLayout* layouts, uint32_t layoutCount);
	VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);
	VkWriteDescriptorSet DescriptorSetWriteBuffer(VkDescriptorType type, VkDescriptorSet destSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding);


}