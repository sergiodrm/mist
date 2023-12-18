#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "VulkanBuffer.h"

namespace vkmmc
{
	// Forward declarations
	struct RenderContext;
	class RenderPipeline;

	class RenderPipelineBuilder
	{
	public:
		// Layout configuration
		VkPipelineLayoutCreateInfo LayoutInfo;

		// Viewport configuration
		VkViewport Viewport;
		VkRect2D Scissor;

		// Stages
		std::vector<VkPipelineShaderStageCreateInfo> ShaderStages;
		// Pipeline states
		VkPipelineRasterizationStateCreateInfo Rasterizer;
		VkPipelineMultisampleStateCreateInfo Multisampler;
		VkPipelineDepthStencilStateCreateInfo DepthStencil;
		// Color attachment
		VkPipelineColorBlendAttachmentState ColorBlendAttachment;

		// Vertex input. How the vertices are arranged in memory and how to bind them.
		VertexInputLayout InputDescription;

		RenderPipeline Build(VkDevice device, VkRenderPass renderPass);
	};

	class RenderPipeline
	{
	public:

		bool SetupPipeline(VkPipeline pipeline, VkPipelineLayout pipelineLayout);
		void Destroy(const RenderContext& renderContext);
		inline bool IsValid() const { return m_pipeline != VK_NULL_HANDLE && m_pipelineLayout != VK_NULL_HANDLE; }

		inline VkPipeline GetPipelineHandle() const { return m_pipeline; }
		inline VkPipelineLayout GetPipelineLayoutHandle() const { return m_pipelineLayout; }

		bool operator==(const RenderPipeline& r) const { return m_pipeline == r.m_pipeline && m_pipelineLayout == r.m_pipelineLayout; }
		bool operator!=(const RenderPipeline& r) const { return !(*this == r); }
	private:
		VkPipeline m_pipeline = VK_NULL_HANDLE;
		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
	};
}