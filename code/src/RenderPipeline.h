#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "RenderTypes.h"

namespace vkmmc
{
	// Forward declarations
	struct RenderContext;
	class RenderPipeline;

	struct ShaderModuleLoadDescription
	{
		std::string ShaderFilePath;
		VkShaderStageFlagBits Flags;
	};

	/**
	 * Create VkShaderModule from a file.
	 * This will compile the shader code in the file.
	 * @return VkShaderModule valid if the compilation terminated successfully. VK_NULL_HANDLE otherwise.
	 */
	VkShaderModule LoadShaderModule(VkDevice device, const char* filename);

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
		VertexInputDescription InputDescription;

		RenderPipeline Build(VkDevice device, VkRenderPass renderPass);
	};

	class RenderPipeline
	{
	public:

		bool SetupPipeline(VkPipeline pipeline, VkPipelineLayout pipelineLayout);
		void Destroy(const RenderContext& renderContext);
		inline bool IsValid() const { return m_pipeline != VK_NULL_HANDLE && m_pipelineLayout != VK_NULL_HANDLE; }
	private:
		VkPipeline m_pipeline = VK_NULL_HANDLE;
		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
	};
}