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
	class RenderPass;
	
	struct ShaderDescription
	{
		std::string Filepath;
		VkShaderStageFlagBits Stage;
	};

	class RenderPipelineBuilder
	{
	public:
		RenderPipelineBuilder(const RenderContext& renderContext);

		const RenderContext& RContext;

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
		// Input assembly type
		VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		// Subpass of renderpass
		uint32_t SubpassIndex = 0;

		RenderPipeline Build(VkRenderPass renderPass);
	};

	class RenderPipeline
	{
	public:

		static RenderPipeline Create(
			RenderContext renderContext,
			const VkRenderPass& renderPass,
			uint32_t subpassIndex,
			const ShaderDescription* shaderStages,
			uint32_t shaderStageCount,
			const VkPipelineLayoutCreateInfo& layoutInfo,
			const VertexInputLayout& inputDescription);

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