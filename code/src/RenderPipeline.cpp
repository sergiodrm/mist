#include "RenderPipeline.h"
#include "VulkanRenderEngine.h"
#include "InitVulkanTypes.h"
#include "GenericUtils.h"
#include "Logger.h"

namespace vkmmc
{
	VkShaderModule LoadShaderModule(VkDevice device, const char* filename)
	{
		VkShaderModule shaderModule = VK_NULL_HANDLE;
		// Read file
		std::vector<uint32_t> spirvBuffer;
		if (vkmmc_utils::ReadFile(filename, spirvBuffer))
		{
			// Create shader
			VkShaderModuleCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.pNext = nullptr;
			createInfo.codeSize = spirvBuffer.size() * sizeof(uint32_t);
			createInfo.pCode = spirvBuffer.data();

			// If fail, shader compilation failed...
			vkmmc_vkcheck(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));
		}
		return shaderModule;
	}

	RenderPipeline RenderPipelineBuilder::Build(VkDevice device, VkRenderPass renderPass)
	{
		// Create VkPipelineLayout from LayoutInfo member
		VkPipelineLayout pipelineLayout;
		vkmmc_vkcheck(vkCreatePipelineLayout(device, &LayoutInfo, nullptr, &pipelineLayout));

		// Create vertex input vulkan structures
		VkPipelineInputAssemblyStateCreateInfo assemblyInfo = PipelineInputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		VkPipelineVertexInputStateCreateInfo inputInfo = PipelineVertexInputStageCreateInfo();
		vkmmc_check(InputDescription.Attributes.size() > 0);
		vkmmc_check(InputDescription.Bindings.size() > 0);
		inputInfo.pVertexAttributeDescriptions = InputDescription.Attributes.data();
		inputInfo.vertexAttributeDescriptionCount = (uint32_t)InputDescription.Attributes.size();
		inputInfo.pVertexBindingDescriptions = InputDescription.Bindings.data();
		inputInfo.vertexBindingDescriptionCount = (uint32_t)InputDescription.Bindings.size();

		// Make viewport state from our stored viewport and scissor.
		// At the moment we won't support multiple viewport and scissor.
		VkPipelineViewportStateCreateInfo viewportStateInfo = {};
		viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateInfo.pNext = nullptr;
		viewportStateInfo.viewportCount = 1;
		viewportStateInfo.pViewports = &Viewport;
		viewportStateInfo.scissorCount = 1;
		viewportStateInfo.pScissors = &Scissor;

		// Blending info
		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.pNext = nullptr;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &ColorBlendAttachment;

		// Build the pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.pNext = nullptr;
		pipelineInfo.stageCount = (uint32_t)ShaderStages.size();
		pipelineInfo.pStages = ShaderStages.data();
		pipelineInfo.pVertexInputState = &inputInfo;
		pipelineInfo.pInputAssemblyState = &assemblyInfo;
		pipelineInfo.pViewportState = &viewportStateInfo;
		pipelineInfo.pRasterizationState = &Rasterizer;
		pipelineInfo.pMultisampleState = &Multisampler;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

		// Build depth buffer
		pipelineInfo.pDepthStencilState = &DepthStencil;

		// Create pipeline and check it's all set up correctly
		VkPipeline newPipeline;
		if (vkCreateGraphicsPipelines(device,
			VK_NULL_HANDLE,
			1, &pipelineInfo,
			nullptr,
			&newPipeline) == VK_SUCCESS)
		{
			Log(LogLevel::Info, "New graphics pipeline created successfuly!\n");
		}
		else
		{
			Log(LogLevel::Error, "Failed to create graphics pipeline.\n");
			newPipeline = VK_NULL_HANDLE;
		}

		RenderPipeline pipelineObject;
		pipelineObject.SetupPipeline(newPipeline, pipelineLayout);
		return pipelineObject;
	}

	bool RenderPipeline::SetupPipeline(VkPipeline pipeline, VkPipelineLayout layout)
	{
		vkmmc_check(m_pipeline == VK_NULL_HANDLE && m_pipelineLayout == VK_NULL_HANDLE);
		m_pipeline = pipeline;
		m_pipelineLayout = layout;
		Log(LogLevel::Info, "Render pipeline setup success.\n");
		return true;
	}

	void RenderPipeline::Destroy(const RenderContext& renderContext)
	{
		Log(LogLevel::Info, "Destroy render pipeline.\n");
		vkDestroyPipeline(renderContext.Device, m_pipeline, nullptr);
		vkDestroyPipelineLayout(renderContext.Device, m_pipelineLayout, nullptr);
		m_pipeline = VK_NULL_HANDLE;
		m_pipelineLayout = VK_NULL_HANDLE;
	}
}