#include "RenderPipeline.h"
#include "VulkanRenderEngine.h"
#include "InitVulkanTypes.h"
#include "GenericUtils.h"
#include "Logger.h"
#include "Debug.h"
#include "Shader.h"

namespace vkmmc
{
	RenderPipelineBuilder::RenderPipelineBuilder(const RenderContext& renderContext)
		: RContext(renderContext)
	{
		// Configure viewport settings.
		Viewport.x = 0.f;
		Viewport.y = 0.f;
		Viewport.width = (float)renderContext.Window->Width;
		Viewport.height = (float)renderContext.Window->Height;
		Viewport.minDepth = 0.f;
		Viewport.maxDepth = 1.f;
		Scissor.offset = { 0, 0 };
		Scissor.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
		// Depth testing
		DepthStencil = vkinit::PipelineDepthStencilCreateInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
		// Rasterization: draw filled triangles
		Rasterizer = vkinit::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
		// Single blenc attachment without blending and writing RGBA
		ColorBlendAttachment = vkinit::PipelineColorBlendAttachmentState();
		// Disable multisampling by default
		Multisampler = vkinit::PipelineMultisampleStateCreateInfo();
	}

	RenderPipeline RenderPipelineBuilder::Build(VkRenderPass renderPass)
	{
		// Create VkPipelineLayout from LayoutInfo member
		VkPipelineLayout pipelineLayout;
		vkcheck(vkCreatePipelineLayout(RContext.Device, &LayoutInfo, nullptr, &pipelineLayout));

		// Create vertex input vulkan structures
		VkPipelineInputAssemblyStateCreateInfo assemblyInfo = vkinit::PipelineInputAssemblyCreateInfo(Topology);
		VkPipelineVertexInputStateCreateInfo inputInfo = vkinit::PipelineVertexInputStageCreateInfo();
		//check(InputDescription.Attributes.size() > 0);
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
		pipelineInfo.subpass = SubpassIndex;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

		// Build depth buffer
		pipelineInfo.pDepthStencilState = &DepthStencil;

		// Create pipeline and check it's all set up correctly
		VkPipeline newPipeline;
		if (vkCreateGraphicsPipelines(RContext.Device,
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

	RenderPipeline RenderPipeline::Create(const RenderContext& renderContext, 
		const VkRenderPass& renderPass,
		uint32_t subpassIndex,
		const ShaderDescription* shaderStages,
		uint32_t shaderStageCount,
		const VertexInputLayout& inputDescription,
		VkPrimitiveTopology topology)
	{
		check(shaderStages && shaderStageCount > 0);
		RenderPipelineBuilder builder(renderContext);
		// Input configuration
		builder.InputDescription = inputDescription;
		builder.SubpassIndex = subpassIndex;
		builder.Topology = topology;
		// Shader stages
		ShaderCompiler compiler(renderContext);
		for (uint32_t i = 0; i < shaderStageCount; ++i)
		{
			compiler.ProcessShaderFile(shaderStages[i].Filepath.c_str(), shaderStages[i].Stage);
			VkShaderModule compiled = compiler.GetCompiledModule(shaderStages[i].Stage);
			builder.ShaderStages.push_back(vkinit::PipelineShaderStageCreateInfo(shaderStages[i].Stage, compiled));
		}
		// TODO: remove singleton dependency
		compiler.GenerateResources(IRenderEngine::GetRenderEngineAs<VulkanRenderEngine>()->GetDescriptorSetLayoutCache());
		// Pass layout info
		builder.LayoutInfo = vkinit::PipelineLayoutCreateInfo();
		builder.LayoutInfo.pushConstantRangeCount = compiler.GetPushConstantCount();
		builder.LayoutInfo.pPushConstantRanges = compiler.GetPushConstantArray();
		builder.LayoutInfo.setLayoutCount = compiler.GetDescriptorSetLayoutCount();
		builder.LayoutInfo.pSetLayouts = compiler.GetDescriptorSetLayoutArray();
		
		// Build the new pipeline
		RenderPipeline renderPipeline = builder.Build(renderPass);

		// Free shader compiler cached data
		compiler.ClearCachedData();

		return renderPipeline;
	}

	RenderPipeline RenderPipeline::Create(
		const RenderContext& renderContext,
		const VkRenderPass& renderPass, 
		uint32_t subpassIndex, 
		const ShaderDescription* shaderStages, 
		uint32_t shaderStageCount, 
		const VkDescriptorSetLayout* layouts, 
		uint32_t layoutCount, 
		const VkPushConstantRange* pushConstants, 
		uint32_t pushConstantCount, 
		const VertexInputLayout& inputDescription, 
		VkPrimitiveTopology topology)
	{
		check(shaderStages && shaderStageCount > 0);
		RenderPipelineBuilder builder(renderContext);
		// Input configuration
		builder.InputDescription = inputDescription;
		builder.SubpassIndex = subpassIndex;
		builder.Topology = topology;
		// Shader stages
		ShaderCompiler compiler(renderContext);
		for (uint32_t i = 0; i < shaderStageCount; ++i)
		{
			compiler.ProcessShaderFile(shaderStages[i].Filepath.c_str(), shaderStages[i].Stage);
			VkShaderModule compiled = compiler.GetCompiledModule(shaderStages[i].Stage);
			builder.ShaderStages.push_back(vkinit::PipelineShaderStageCreateInfo(shaderStages[i].Stage, compiled));
		}
		// Pass layout info
		builder.LayoutInfo = vkinit::PipelineLayoutCreateInfo();
		builder.LayoutInfo.pushConstantRangeCount = pushConstantCount;
		builder.LayoutInfo.pPushConstantRanges = pushConstants;
		builder.LayoutInfo.setLayoutCount = layoutCount;
		builder.LayoutInfo.pSetLayouts = layouts;

		// Build the new pipeline
		RenderPipeline renderPipeline = builder.Build(renderPass);

		// Free shader compiler cached data
		compiler.ClearCachedData();

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