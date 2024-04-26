// src file for Mist project 

#include "Shader.h"

//#include <spirv-cross-main/spirv_glsl.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include "RenderTypes.h"
#include "GenericUtils.h"
#include "Debug.h"
#include "InitVulkanTypes.h"
#include "RenderDescriptor.h"
#include "Logger.h"
#include "RenderContext.h"
#include "VulkanRenderEngine.h"

#include "TimeUtils.h"



#ifdef SHADER_RUNTIME_COMPILATION

#include <shaderc/shaderc.hpp>  


namespace shader_compiler
{
	struct tCompilationResult
	{
		uint32_t* binary = nullptr;
		size_t binaryCount = 0;

		inline bool IsCompilationSucceed() const { return binary && binaryCount; }
	};

	void FreeBinary(tCompilationResult& res)
	{
		delete[] res.binary;
		res.binary = nullptr;
		res.binaryCount = 0;
	}

	shaderc_shader_kind GetShaderType(VkShaderStageFlags flags)
	{
		switch (flags)
		{
		case VK_SHADER_STAGE_VERTEX_BIT: return shaderc_glsl_vertex_shader;
		case VK_SHADER_STAGE_FRAGMENT_BIT: return shaderc_glsl_fragment_shader;
		}
		check(false && "Unreachable code");
		return (shaderc_shader_kind)0;
	}

	const char* GetShaderStatusStr(shaderc_compilation_status status)
	{
		switch (status)
		{
		case shaderc_compilation_status_success: return "success";
		case shaderc_compilation_status_invalid_stage: return "invalid_stage";
		case shaderc_compilation_status_compilation_error: return "compilation_error";
		case shaderc_compilation_status_internal_error: return "internal_error";
		case shaderc_compilation_status_null_result_object: return "null_result_object";
		case shaderc_compilation_status_invalid_assembly: return "invalid_assembly";
		case shaderc_compilation_status_validation_error: return "validation_error";
		case shaderc_compilation_status_transformation_error: return "transformation_error";
		case shaderc_compilation_status_configuration_error: return "configurarion_error";
		}
		return "unknown";
	}

	template<typename T>
	bool HandleError(const shaderc::CompilationResult<T>& res, const char* compilationStage)
	{
		shaderc_compilation_status status = res.GetCompilationStatus();
		Mist::LogLevel level = status == shaderc_compilation_status_success ? Mist::LogLevel::Ok : Mist::LogLevel::Error;
		Mist::Logf(level, "-- Shader %s result (%s): %d warnings, %d errors --\n", compilationStage, GetShaderStatusStr(status), res.GetNumWarnings(), res.GetNumErrors());
		if (status != shaderc_compilation_status_success)
		{
			Mist::Logf(Mist::LogLevel::Error, "Shader compilation error: %s\n", res.GetErrorMessage().c_str());
			return false;
		}
		return true;
	}

	tCompilationResult Compile(const char* filepath, VkShaderStageFlags stage)
	{
		char* source;
		size_t s;
		check(Mist::io::ReadFile(filepath, &source, s));

		shaderc::Compiler compiler;
		shaderc::CompileOptions options;

		// Like -DMY_DEFINE=1
		//options.AddMacroDefinition("MY_DEFINE", "1");
		//options.SetOptimizationLevel(shaderc_optimization_level_size);
		options.SetGenerateDebugInfo();
		shaderc_shader_kind kind = GetShaderType(stage);


		shaderc::PreprocessedSourceCompilationResult prepRes = compiler.PreprocessGlsl(source, s, kind, filepath, options);
		if (!HandleError(prepRes, "preprocess"))
			return tCompilationResult();

		Mist::tString preprocessSource{ prepRes.cbegin(), prepRes.cend() };
		shaderc::AssemblyCompilationResult result = compiler.CompileGlslToSpvAssembly(preprocessSource.c_str(),preprocessSource.size(), kind, filepath, "main", options);
		if (!HandleError(result, "assembly"))
			return tCompilationResult();
		std::string assemble{ result.begin(), result.end() };
		shaderc::SpvCompilationResult spv = compiler.AssembleToSpv(assemble);
		if (!HandleError(spv, "assembly to spv"))
			return tCompilationResult();

		tCompilationResult compResult;
		compResult.binaryCount = (spv.cend() - spv.cbegin());
		compResult.binary = (uint32_t*)malloc(compResult.binaryCount * sizeof(uint32_t));
		memcpy_s(compResult.binary, compResult.binaryCount * sizeof(uint32_t), spv.cbegin(), compResult.binaryCount * sizeof(uint32_t));
		return compResult;
	}
}

#endif // SHADER_RUNTIME_COMPILATION



namespace vkutils
{
	VkBufferUsageFlags GetVulkanBufferUsage(VkDescriptorType type)
	{
		switch (type)
		{
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: 
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		}
		return VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
	}

	const char* GetVulkanShaderStageName(VkShaderStageFlags shaderStage)
	{
		switch (shaderStage)
		{
		case VK_SHADER_STAGE_VERTEX_BIT: return "Vertex";
		case VK_SHADER_STAGE_FRAGMENT_BIT: return "Fragment";
		}
		check(false && "Unreachable code");
		return "Unknown";
	}

	const char* GetVulkanDescriptorTypeName(VkDescriptorType type)
	{
		switch (type)
		{
		case VK_DESCRIPTOR_TYPE_SAMPLER: return "SAMPLER";
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return "COMBINED_IMAGE_SAMPLER";
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return "SAMPLED_IMAGE";
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: return "STORAGE_IMAGE";
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: return "UNIFORM_TEXEL_BUFFER";
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: return "STORAGE_TEXEL_BUFFER";
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return "UNIFORM_BUFFER";
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: return "STORAGE_BUFFER";
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return "UNIFORM_BUFFER_DYNAMIC";
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return "STORAGE_BUFFER_DYNAMIC";
		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return "INPUT_ATTACHMENT";
		case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK: return "INLINE_UNIFORM_BLOCK";
		case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: return "ACCELERATION_STRUCTURE_KHR";
		case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV: return "ACCELERATION_STRUCTURE_NV";
		case VK_DESCRIPTOR_TYPE_MUTABLE_VALVE: return "MUTABLE_VALVE";
		case VK_DESCRIPTOR_TYPE_MAX_ENUM:
		default:
			check(false && "Invalid descriptor type.");
			break;
		}
		return "";
	}
}

namespace Mist
{


	ShaderCompiler::ShaderCompiler(const RenderContext& renderContext) : m_renderContext(renderContext)
	{}

	ShaderCompiler::~ShaderCompiler()
	{
		ClearCachedData();
	}

	void ShaderCompiler::ClearCachedData()
	{
		for (auto it : m_modules)
		{
			vkDestroyShaderModule(m_renderContext.Device, it.CompiledModule, nullptr);
		}
		m_modules.clear();
		m_cachedLayoutArray.clear();
		m_cachedPushConstantArray.clear();
	}

	bool ShaderCompiler::ProcessShaderFile(const char* filepath, VkShaderStageFlagBits shaderStage)
	{
		tTimePoint start = GetTimePoint();
		Logf(LogLevel::Info, "Compiling shader: [%s: %s]\n", vkutils::GetVulkanShaderStageName(shaderStage), filepath);

		// integrity check: dont repeat shader stage and ensure correct file extension with shader stage.
		check(GetCompiledModule(shaderStage) == VK_NULL_HANDLE);
		check(CheckShaderFileExtension(filepath, shaderStage));

#ifdef SHADER_RUNTIME_COMPILATION
		shader_compiler::tCompilationResult bin = shader_compiler::Compile(filepath, shaderStage);
		if (!bin.IsCompilationSucceed())
			return false;
#else
		// Read shader source and convert it to spirv binary
		ShaderFileContent bin;
		check(CacheSourceFromFile(filepath, content));
		check(AssembleShaderSource(content.Raw, content.RawSize, shaderStage, &content.Assembled, &content.AssembledSize));
		// Create reflection info
		ProcessReflection(shaderStage, content.Assembled, content.AssembledSize);
#endif // SHADER_RUNTIME_COMPILATION

		// Create vk module
		CompiledShaderModule data;
		data.ShaderStage = shaderStage;
		data.CompiledModule = CompileShaderModule(m_renderContext, bin.binary, bin.binaryCount, data.ShaderStage);
		m_modules.push_back(data);
		ProcessReflection(shaderStage, bin.binary, bin.binaryCount);

#ifdef SHADER_RUNTIME_COMPILATION
		// release resources  
		shader_compiler::FreeBinary(bin);
#endif // SHADER_RUNTIME_COMPILATION

		tTimePoint end = GetTimePoint();
		float ms = GetMiliseconds(end - start);
		Logf(LogLevel::Ok, "Shader compiled successfully (%s: %s) [time lapsed: %f ms]\n", vkutils::GetVulkanShaderStageName(shaderStage), filepath, ms);
		return true;
	}

	VkShaderModule ShaderCompiler::GetCompiledModule(VkShaderStageFlags shaderStage) const
	{
		for (size_t i = 0; i < m_modules.size(); ++i)
		{
			if (m_modules[i].ShaderStage == shaderStage)
			{
				// Cached modules must be compiled
				check(m_modules[i].CompiledModule != VK_NULL_HANDLE);
				return m_modules[i].CompiledModule;
			}
		}
		return VK_NULL_HANDLE;
	}

	bool ShaderCompiler::GenerateReflectionResources(DescriptorLayoutCache& layoutCache)
	{
		check(m_cachedLayoutArray.empty() && m_cachedPushConstantArray.empty() && "Compiler already has cached data. Compile all shaders before generate resources.");
		m_cachedLayoutArray.resize(m_reflectionProperties.DescriptorSetInfoArray.size());
		for (uint32_t i = 0; i < (uint32_t)m_reflectionProperties.DescriptorSetInfoArray.size(); ++i)
		{
			VkDescriptorSetLayout layout = GenerateDescriptorSetLayout(m_reflectionProperties.DescriptorSetInfoArray[i], layoutCache);
			m_cachedLayoutArray[i] = layout;
		}
		for (const auto& it : m_reflectionProperties.PushConstantMap)
			m_cachedPushConstantArray.push_back(GeneratePushConstantInfo(it.second));

		return true;
	}

	VkShaderModule ShaderCompiler::CompileShaderModule(const RenderContext& context, const uint32_t* binaryData, size_t binaryCount, VkShaderStageFlags stage)
	{
		check(binaryData && binaryCount);
		VkShaderModuleCreateInfo info{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .pNext = nullptr };
		info.codeSize = binaryCount * sizeof(uint32_t);
		info.pCode = binaryData;
		VkShaderModule shaderModule{ VK_NULL_HANDLE };
		vkcheck(vkCreateShaderModule(context.Device, &info, nullptr, &shaderModule));
		return shaderModule;
	}

	void ShaderCompiler::ProcessReflection(VkShaderStageFlags shaderStage, uint32_t* binaryData, size_t binaryCount)
	{
		auto processSpirvResource = [this](const spirv_cross::CompilerGLSL& compiler, const spirv_cross::Resource& resource, VkShaderStageFlags shaderStage, VkDescriptorType descriptorType)
			{
				ShaderBindingDescriptorInfo bufferInfo;
				bufferInfo.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
				bufferInfo.Name = resource.name.c_str();
				if (descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
				{
					const spirv_cross::SPIRType& bufferType = compiler.get_type(resource.base_type_id);
					bufferInfo.Size = (uint32_t)compiler.get_declared_struct_size(bufferType);
					bufferInfo.ArrayCount = 1;
				}
				else
				{
					const spirv_cross::SPIRType& type = compiler.get_type(resource.type_id);
					bufferInfo.Size = 0;
					bufferInfo.ArrayCount = type.array.size() > 0 ? type.array[0] : 1;
				}
				bufferInfo.Type = descriptorType;
				bufferInfo.Stage = shaderStage;

				uint32_t setIndex = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				ShaderDescriptorSetInfo& descriptorSetInfo = FindOrCreateDescriptorSet(setIndex);
				if (descriptorSetInfo.BindingArray.size() < bufferInfo.Binding + 1)
					descriptorSetInfo.BindingArray.resize(bufferInfo.Binding + 1);
				descriptorSetInfo.BindingArray[bufferInfo.Binding] = bufferInfo;

				Logf(LogLevel::Debug, "> %s [ShaderStage: %s; Name:%s; Set: %u; Binding: %u; Size: %u; ArrayCount: %u;]\n", vkutils::GetVulkanDescriptorTypeName(descriptorType),
					vkutils::GetVulkanShaderStageName(shaderStage), bufferInfo.Name.c_str(), setIndex, bufferInfo.Binding, bufferInfo.Size, bufferInfo.ArrayCount);
				return setIndex;
			};

		check(binaryData && binaryCount && "Invalid binary source.");
		spirv_cross::CompilerGLSL compiler(binaryData, binaryCount);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		Log(LogLevel::Debug, "Processing shader reflection...\n");

		for (const spirv_cross::Resource& resource : resources.uniform_buffers)
			processSpirvResource(compiler, resource, shaderStage, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

		for (const spirv_cross::Resource& resource : resources.storage_buffers)
			processSpirvResource(compiler, resource, shaderStage, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		for (const spirv_cross::Resource& resource : resources.sampled_images)
			processSpirvResource(compiler, resource, shaderStage, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

		for (const spirv_cross::Resource& resource : resources.push_constant_buffers)
		{
			check(!m_reflectionProperties.PushConstantMap.contains(shaderStage));
			ShaderPushConstantBufferInfo info;
			info.Name = resource.name.c_str();
			info.Offset = compiler.get_decoration(resource.id, spv::DecorationOffset);
			const spirv_cross::SPIRType& type = compiler.get_type(resource.type_id);
			info.Size = (uint32_t)compiler.get_declared_struct_size(type);
			info.ShaderStage = shaderStage;
			m_reflectionProperties.PushConstantMap[shaderStage] = info;
			Logf(LogLevel::Debug, "> PUSH_CONSTANT [ShaderStage: %s; Name: %s; Offset: %zd; Size: %zd]\n",
				vkutils::GetVulkanShaderStageName(shaderStage), info.Name.c_str(), info.Offset, info.Size);
		}

		Log(LogLevel::Debug, "End shader reflection.\n");
	}

	ShaderDescriptorSetInfo& ShaderCompiler::FindOrCreateDescriptorSet(uint32_t set)
	{
		for (uint32_t i = 0; i < (uint32_t)m_reflectionProperties.DescriptorSetInfoArray.size(); ++i)
		{
			if (m_reflectionProperties.DescriptorSetInfoArray[i].SetIndex == set)
				return m_reflectionProperties.DescriptorSetInfoArray[i];
		}

		ShaderDescriptorSetInfo info;
		info.SetIndex = set;
		m_reflectionProperties.DescriptorSetInfoArray.push_back(info);
		return m_reflectionProperties.DescriptorSetInfoArray.back();
	}

	const ShaderDescriptorSetInfo& ShaderCompiler::GetDescriptorSet(uint32_t set) const
	{
		for (uint32_t i = 0; i < (uint32_t)m_reflectionProperties.DescriptorSetInfoArray.size(); ++i)
		{
			if (m_reflectionProperties.DescriptorSetInfoArray[i].SetIndex == set)
				return m_reflectionProperties.DescriptorSetInfoArray[i];
		}
		check(false && "ShaderDescriptorSetInfo set index not found.");
		return m_reflectionProperties.DescriptorSetInfoArray[0];
	}

	VkDescriptorSetLayout ShaderCompiler::GenerateDescriptorSetLayout(const ShaderDescriptorSetInfo& setInfo, DescriptorLayoutCache& layoutCache) const
	{
		VkDescriptorSetLayout setLayout{ VK_NULL_HANDLE };
		std::vector<VkDescriptorSetLayoutBinding> bindingArray(setInfo.BindingArray.size());
		for (uint32_t i = 0; i < setInfo.BindingArray.size(); ++i)
		{
			bindingArray[i].binding = i;
			bindingArray[i].descriptorCount = setInfo.BindingArray[i].ArrayCount;
			bindingArray[i].descriptorType = setInfo.BindingArray[i].Type;
			bindingArray[i].pImmutableSamplers = nullptr;
			bindingArray[i].stageFlags = setInfo.BindingArray[i].Stage;
		}
		VkDescriptorSetLayoutCreateInfo createInfo = vkinit::DescriptorSetLayoutCreateInfo(bindingArray.data(), (uint32_t)bindingArray.size());
		setLayout = layoutCache.CreateLayout(createInfo);
		return setLayout;
	}

	VkPushConstantRange ShaderCompiler::GeneratePushConstantInfo(const ShaderPushConstantBufferInfo& pushConstantInfo) const
	{
		VkPushConstantRange range;
		range.offset = pushConstantInfo.Offset;
		range.size = pushConstantInfo.Size;
		range.stageFlags = pushConstantInfo.ShaderStage;
		return range;
	}

	bool ShaderCompiler::CheckShaderFileExtension(const char* filepath, VkShaderStageFlags shaderStage)
	{
		bool res = false;
		if (filepath && *filepath)
		{
			const char* desiredExt;
			switch (shaderStage)
			{
			case VK_SHADER_STAGE_FRAGMENT_BIT: desiredExt = SHADER_FRAG_FILE_EXTENSION; break;
			case VK_SHADER_STAGE_VERTEX_BIT: desiredExt = SHADER_VERTEX_FILE_EXTENSION; break;
			default:
				return res;
			}
			size_t filepathLen = strlen(filepath);
			size_t desiredExtLen = strlen(desiredExt);
			if (filepathLen > desiredExtLen)
			{
				size_t ext = filepathLen - desiredExtLen;
				res = !strcmp(&filepath[ext], desiredExt);
			}
		}
		return res;
	}

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
		tArray<ShaderDescription, 2> descs;
		descs[0] = { m_description.VertexShaderFile.c_str(), VK_SHADER_STAGE_VERTEX_BIT };
		descs[1] = { m_description.FragmentShaderFile.c_str(), VK_SHADER_STAGE_FRAGMENT_BIT };
		ShaderCompiler compiler(context);
		for (uint32_t i = 0; i < (uint32_t)descs.size(); ++i)
		{
			if (!descs[i].Filepath.empty())
			{
				compiler.ProcessShaderFile(descs[i].Filepath.c_str(), descs[i].Stage);
				VkShaderModule compiled = compiler.GetCompiledModule(descs[i].Stage);
				builder.ShaderStages.push_back(vkinit::PipelineShaderStageCreateInfo(descs[i].Stage, compiled));
			}
		}
		compiler.GenerateReflectionResources(*const_cast<DescriptorLayoutCache*>(context.LayoutCache));

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
		++Mist_profiling::GRenderStats.ShaderProgramCount;
	}

	void ShaderProgram::BindDescriptorSets(VkCommandBuffer cmd, const VkDescriptorSet* setArray, uint32_t setCount, uint32_t firstSet, const uint32_t* dynamicOffsetArray, uint32_t dynamicOffsetCount)
	{
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, firstSet, setCount, setArray, dynamicOffsetCount, dynamicOffsetArray);
		++Mist_profiling::GRenderStats.SetBindingCount;
	}

	void ShaderFileDB::AddShaderProgram(const RenderContext& context, ShaderProgram* program)
	{
		const ShaderProgramDescription& description = program->GetDescription();
		check(!FindShaderProgram(description.VertexShaderFile.c_str(), description.FragmentShaderFile.c_str()));

		uint32_t index = (uint32_t)m_shaderArray.size();
		m_shaderArray.push_back(program);
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
			p = m_shaderArray[m_indexMap.at(key)];
		return p;
	}

	void ShaderFileDB::ReloadFromFile(const RenderContext& context)
	{
		for (uint32_t i = 0; i < (uint32_t)m_shaderArray.size(); ++i)
		{
			m_shaderArray[i]->Destroy(context);
			m_shaderArray[i]->Reload(context);
		}
	}

	void ShaderFileDB::Init(const RenderContext& context)
	{
#ifdef SHADER_RUNTIME_COMPILATION
		
#endif // SHADER_RUNTIME_COMPILATION

	}

	void ShaderFileDB::Destroy(const RenderContext& context)
	{
		for (uint32_t i = 0; i < (uint32_t)m_shaderArray.size(); ++i)
		{
			m_shaderArray[i]->Destroy(context);
			delete m_shaderArray[i];
			m_shaderArray[i] = nullptr;
		}
		m_shaderArray.clear();
		m_indexMap.clear();
#ifdef SHADER_RUNTIME_COMPILATION
#endif // SHADER_RUNTIME_COMPILATION
	}

}
