#include "RenderPipeline.h"
#include "VulkanRenderEngine.h"
#include "InitVulkanTypes.h"
#include "GenericUtils.h"
#include "Logger.h"
#include "Debug.h"
#include "Shader.h"

namespace vkmmc
{

	RenderPipeline RenderPipelineBuilder::Build(VkDevice device, VkRenderPass renderPass)
	{
		// Create VkPipelineLayout from LayoutInfo member
		VkPipelineLayout pipelineLayout;
		vkcheck(vkCreatePipelineLayout(device, &LayoutInfo, nullptr, &pipelineLayout));

		// Create vertex input vulkan structures
		VkPipelineInputAssemblyStateCreateInfo assemblyInfo = vkinit::PipelineInputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		VkPipelineVertexInputStateCreateInfo inputInfo = vkinit::PipelineVertexInputStageCreateInfo();
		check(InputDescription.Attributes.size() > 0);
		inputInfo.pVertexAttributeDescriptions = InputDescription.Attributes.data();
		inputInfo.vertexAttributeDescriptionCount = (uint32_t)InputDescription.Attributes.size();
		inputInfo.pVertexBindingDescriptions = &InputDescription.Binding;
		inputInfo.vertexBindingDescriptionCount = 1;

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

	RenderPipeline RenderPipeline::Create(RenderContext renderContext, 
		const RenderPass& renderPass,
		const ShaderModuleLoadDescription* shaderStages, 
		uint32_t shaderStageCount, 
		const VkPipelineLayoutCreateInfo& layoutInfo, 
		const VertexInputLayout& inputDescription)
	{
		check(shaderStages && shaderStageCount > 0);
		RenderPipelineBuilder builder;
		// Input configuration
		builder.InputDescription = inputDescription;
		// Shader stages
		std::vector<VkShaderModule> modules(shaderStageCount);
		ShaderCompiler compiler(shaderStages, (uint32_t)shaderStageCount);

		compiler.ProcessReflectionProperties();

		compiler.Compile(renderContext, modules);
		for (size_t i = 0; i < shaderStageCount; ++i)
		{
			check(modules[i] != VK_NULL_HANDLE);
			builder.ShaderStages.push_back(
				vkinit::PipelineShaderStageCreateInfo(shaderStages[i].Flags, modules[i]));
		}
		// Configure viewport settings.
		builder.Viewport.x = 0.f;
		builder.Viewport.y = 0.f;
		builder.Viewport.width = (float)renderContext.Width;
		builder.Viewport.height = (float)renderContext.Height;
		builder.Viewport.minDepth = 0.f;
		builder.Viewport.maxDepth = 1.f;
		builder.Scissor.offset = { 0, 0 };
		builder.Scissor.extent = { .width = renderContext.Width, .height = renderContext.Height };
		// Depth testing
		builder.DepthStencil = vkinit::PipelineDepthStencilCreateInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
		// Rasterization: draw filled triangles
		builder.Rasterizer = vkinit::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
		// Single blenc attachment without blending and writing RGBA
		builder.ColorBlendAttachment = vkinit::PipelineColorBlendAttachmentState();
		//builder.ColorBlendAttachment.blendEnable = VK_TRUE;
		//builder.ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		//builder.ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		//builder.ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		//builder.ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		//builder.ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		//builder.ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_SUBTRACT;
		// Disable multisampling by default
		builder.Multisampler = vkinit::PipelineMultisampleStateCreateInfo();
		// Pass layout info
		builder.LayoutInfo = layoutInfo;

		// Build the new pipeline
		RenderPipeline renderPipeline = builder.Build(renderContext.Device, renderPass.GetRenderPassHandle());;

		// Vulkan modules destruction
		for (size_t i = 0; i < shaderStageCount; ++i)
			vkDestroyShaderModule(renderContext.Device, modules[i], nullptr);
		modules.clear();

		return renderPipeline;
	}

	bool RenderPipeline::SetupPipeline(VkPipeline pipeline, VkPipelineLayout layout)
	{
		check(m_pipeline == VK_NULL_HANDLE && m_pipelineLayout == VK_NULL_HANDLE);
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