#include "RenderPipeline.h"
#include "VulkanRenderEngine.h"
#include "InitVulkanTypes.h"
#include "GenericUtils.h"
#include "Logger.h"
#include "Debug.h"
#include "Shader.h"

namespace vkmmc
{
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
		std::vector<VkPipelineColorBlendAttachmentState> ColorBlendAttachment;

		// Vertex input. How the vertices are arranged in memory and how to bind them.
		VertexInputLayout InputDescription;
		// Input assembly type
		VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		// Subpass of renderpass
		uint32_t SubpassIndex = 0;

		void Build(VkRenderPass renderPass, VkPipeline& pipeline, VkPipelineLayout& layout);
	};

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
		// Disable multisampling by default
		Multisampler = vkinit::PipelineMultisampleStateCreateInfo();
	}

	void RenderPipelineBuilder::Build(VkRenderPass renderPass, VkPipeline& pipeline, VkPipelineLayout& layout)
	{
		// Create VkPipelineLayout from LayoutInfo member
		vkcheck(vkCreatePipelineLayout(RContext.Device, &LayoutInfo, nullptr, &layout));

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
		colorBlending.attachmentCount = (uint32_t)ColorBlendAttachment.size();
		colorBlending.pAttachments = ColorBlendAttachment.data();

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
		pipelineInfo.layout = layout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = SubpassIndex;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

		// Build depth buffer
		pipelineInfo.pDepthStencilState = &DepthStencil;

		// Create pipeline and check it's all set up correctly
		vkcheck(vkCreateGraphicsPipelines(RContext.Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
	}

	ShaderProgram::ShaderProgram()
		: m_pipeline(VK_NULL_HANDLE)
		, m_pipelineLayout(VK_NULL_HANDLE)
	{	}

	ShaderProgram* ShaderProgram::Create(const RenderContext& context, const ShaderProgramDescription& description)
	{
		ShaderProgram* program = new ShaderProgram();
		check(program->_Create(context, description));
		ShaderFileDB& db = *const_cast<ShaderFileDB*>(context.ShaderDB);
		db.AddShaderProgram(context, program);
		return program;
	}

	bool ShaderProgram::_Create(const RenderContext& context, const ShaderProgramDescription& description)
	{
		check(!IsLoaded());
		m_description = description;
		return Reload(context);
	}

	void ShaderProgram::Destroy(const RenderContext& context)
	{
		check(IsLoaded());
		vkDestroyPipelineLayout(context.Device, m_pipelineLayout, nullptr);
		vkDestroyPipeline(context.Device, m_pipeline, nullptr);
	}

	bool ShaderProgram::Reload(const RenderContext& context)
	{
		check(!m_description.VertexShaderFile.empty() || !m_description.FragmentShaderFile.empty());
		check(m_description.RenderTarget);
		RenderPipelineBuilder builder(context);
		// Input configuration
		builder.InputDescription = m_description.InputLayout;
		builder.SubpassIndex = m_description.SubpassIndex;
		builder.Topology = m_description.Topology;
		// Shader stages
		ShaderCompiler compiler(context);
		tArray<ShaderDescription, 2> descs;
		descs[0] = { m_description.VertexShaderFile.c_str(), VK_SHADER_STAGE_VERTEX_BIT };
		descs[1] = { m_description.FragmentShaderFile.c_str(), VK_SHADER_STAGE_FRAGMENT_BIT };
		for (uint32_t i = 0; i < (uint32_t)descs.size(); ++i)
		{
			if (!descs[i].Filepath.empty())
			{
				compiler.ProcessShaderFile(descs[i].Filepath.c_str(), descs[i].Stage);
				VkShaderModule compiled = compiler.GetCompiledModule(descs[i].Stage);
				builder.ShaderStages.push_back(vkinit::PipelineShaderStageCreateInfo(descs[i].Stage, compiled));
			}
		}
		compiler.GenerateResources(*const_cast<DescriptorLayoutCache*>(context.LayoutCache));

		// Blending info for color attachments
		uint32_t colorAttachmentCount = m_description.RenderTarget->GetDescription().ColorAttachmentCount;
		builder.ColorBlendAttachment.resize(colorAttachmentCount);
		for (uint32_t i = 0; i < colorAttachmentCount; ++i)
		{
			// Single blenc attachment without blending and writing RGBA
			builder.ColorBlendAttachment[i] = vkinit::PipelineColorBlendAttachmentState();
		}

		// Pass layout info
		builder.LayoutInfo = vkinit::PipelineLayoutCreateInfo();
		if (m_description.SetLayoutArray && m_description.SetLayoutCount)
		{
			builder.LayoutInfo.setLayoutCount = m_description.SetLayoutCount;
			builder.LayoutInfo.pSetLayouts = m_description.SetLayoutArray;
		}
		else
		{
			builder.LayoutInfo.setLayoutCount = compiler.GetDescriptorSetLayoutCount();
			builder.LayoutInfo.pSetLayouts = compiler.GetDescriptorSetLayoutArray();
		}
		if (m_description.PushConstantArray && m_description.PushConstantCount)
		{
			builder.LayoutInfo.pushConstantRangeCount = m_description.PushConstantCount;
			builder.LayoutInfo.pPushConstantRanges = m_description.PushConstantArray;
		}
		else
		{
			builder.LayoutInfo.pushConstantRangeCount = compiler.GetPushConstantCount();
			builder.LayoutInfo.pPushConstantRanges = compiler.GetPushConstantArray();
		}

		// Build the new pipeline
		builder.Build(m_description.RenderTarget->GetRenderPass(), m_pipeline, m_pipelineLayout);

		// Free shader compiler cached data
		compiler.ClearCachedData();

		return true;
	}

	void ShaderProgram::UseProgram(VkCommandBuffer cmd)
	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
		++vkmmc_profiling::GRenderStats.ShaderProgramCount;
	}

	void ShaderProgram::BindDescriptorSets(VkCommandBuffer cmd, const VkDescriptorSet* setArray, uint32_t setCount, uint32_t firstSet, const uint32_t* dynamicOffsetArray, uint32_t dynamicOffsetCount)
	{
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, firstSet, setCount, setArray, dynamicOffsetCount, dynamicOffsetArray);
		++vkmmc_profiling::GRenderStats.SetBindingCount;
	}

	void ShaderFileDB::AddShaderProgram(const RenderContext& context, ShaderProgram* program)
	{
		const ShaderProgramDescription& description = program->GetDescription();
		check(!FindShaderProgram(description.VertexShaderFile.c_str(), description.FragmentShaderFile.c_str()));

		uint32_t index = (uint32_t)m_pipelineArray.size();
		m_pipelineArray.push_back(program);
		char key[512];
		GenerateKey(key, description.VertexShaderFile.c_str(), description.FragmentShaderFile.c_str());
		m_indexMap[key] = index;
	}

	ShaderProgram* ShaderFileDB::FindShaderProgram(const char* vertexFile, const char* fragmentFile) const
	{
		ShaderProgram* p = nullptr;
		char key[512];
		GenerateKey(key, vertexFile, fragmentFile);
		if (m_indexMap.contains(key))
			p = m_pipelineArray[m_indexMap.at(key)];
		return p;
	}

	void ShaderFileDB::Destroy(const RenderContext& context)
	{
		for (uint32_t i = 0; i < (uint32_t)m_pipelineArray.size(); ++i)
		{
			m_pipelineArray[i]->Destroy(context);
			delete m_pipelineArray[i];
			m_pipelineArray[i] = nullptr;
		}
		m_pipelineArray.clear();
		m_indexMap.clear();
	}

}