// src file for Mist project 

#include "Render/Shader.h"

//#include <spirv-cross-main/spirv_glsl.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include "Render/RenderTypes.h"
#include "Utils/GenericUtils.h"
#include "Core/Debug.h"
#include "Render/InitVulkanTypes.h"
#include "Render/RenderDescriptor.h"
#include "Core/Logger.h"
#include "Render/RenderContext.h"
#include "Render/VulkanRenderEngine.h"
#include "Application/Application.h"

#include "Utils/TimeUtils.h"



#ifdef SHADER_RUNTIME_COMPILATION

//#define MIST_SHADER_REFLECTION_LOG
//#define SHADER_FORCE_COMPILATION
//#define SHADER_DUMP_PREPROCESS_RESULT
#define SHADER_BINARY_FILE_EXTENSION ".spv"

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
		case VK_SHADER_STAGE_COMPUTE_BIT: return shaderc_glsl_compute_shader;
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

	tCompilationResult Compile(const char* filepath, VkShaderStageFlags stage, const Mist::tCompileOptions* compileOptions = nullptr)
	{
		char* source;
		size_t s;
		check(Mist::io::ReadFile(filepath, &source, s));
#ifdef SHADER_DUMP_PREPROCESS_RESULT
		Mist::Logf(Mist::LogLevel::Debug, "Preprocess: %s\n");
#endif

		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		const char* entryPoint = "main";

		if (compileOptions)
		{
#ifdef SHADER_DUMP_PREPROCESS_RESULT
			Mist::Logf(Mist::LogLevel::Debug, "Macro definition:\n");
#endif
			for (uint32_t i = 0; i < (size_t)compileOptions->MacroDefinitionArray.size(); ++i)
			{
				check(*compileOptions->MacroDefinitionArray[i].Macro);
				if (*compileOptions->MacroDefinitionArray[i].Value)
					options.AddMacroDefinition(compileOptions->MacroDefinitionArray[i].Macro, compileOptions->MacroDefinitionArray[i].Value);
				else
					options.AddMacroDefinition(compileOptions->MacroDefinitionArray[i].Macro);
#ifdef SHADER_DUMP_PREPROCESS_RESULT
				Mist::Logf(Mist::LogLevel::Debug, "> %s (%s)", compileOptions->MacroDefinitionArray[i].Macro, compileOptions->MacroDefinitionArray[i].Value);
#endif
			}
			if (compileOptions->GenerateDebugInfo)
				options.SetGenerateDebugInfo();
			if (*compileOptions->EntryPointName)
				entryPoint = compileOptions->EntryPointName;
		}
		else
		{
			options.SetGenerateDebugInfo();
			//options.SetOptimizationLevel(shaderc_optimization_level_size);
		}
#ifdef SHADER_DUMP_PREPROCESS_RESULT
		Mist::Logf(Mist::LogLevel::Debug, "Entry point: %s\n", entryPoint);
#endif

		shaderc_shader_kind kind = GetShaderType(stage);
		shaderc::PreprocessedSourceCompilationResult prepRes = compiler.PreprocessGlsl(source, s, kind, filepath, options);
		if (!HandleError(prepRes, "preprocess"))
			return tCompilationResult();

		Mist::tString preprocessSource{ prepRes.cbegin(), prepRes.cend() };
#ifdef SHADER_DUMP_PREPROCESS_RESULT
		Mist::Logf(Mist::LogLevel::Debug, "Preprocessed source:\n\n%s\n\n", preprocessSource.c_str());
#endif
		shaderc::AssemblyCompilationResult result = compiler.CompileGlslToSpvAssembly(preprocessSource.c_str(), preprocessSource.size(), kind, filepath, entryPoint, options);
		if (!HandleError(result, "assembly"))
			return tCompilationResult();
		Mist::tString assemble{ result.begin(), result.end() };
		shaderc::SpvCompilationResult spv = compiler.AssembleToSpv(assemble);
		if (!HandleError(spv, "assembly to spv"))
			return tCompilationResult();

		tCompilationResult compResult;
		compResult.binaryCount = (spv.cend() - spv.cbegin());
		compResult.binary = (uint32_t*)malloc(compResult.binaryCount * sizeof(uint32_t));
		memcpy_s(compResult.binary, compResult.binaryCount * sizeof(uint32_t), spv.cbegin(), compResult.binaryCount * sizeof(uint32_t));
		return compResult;
	}

	template <uint32_t Size>
	void GenerateSpvFileName(char(&outFilepath)[Size], const char* srcFile, const Mist::tCompileOptions& options)
	{
		sprintf_s(outFilepath, Size, "%s.[%s]", srcFile, options.EntryPointName);
		if (!options.MacroDefinitionArray.empty())
		{
			strcat_s(outFilepath, Size, ".[");
			for (uint32_t i = 0; i < (uint32_t)options.MacroDefinitionArray.size(); ++i)
			{
				char buff[256];
				sprintf_s(buff, "%s(%s).", options.MacroDefinitionArray[i].Macro, options.MacroDefinitionArray[i].Value);
				strcat_s(outFilepath, Size, buff);
			}
			strcat_s(outFilepath, Size, "]");
		}
		strcat_s(outFilepath, Size, SHADER_BINARY_FILE_EXTENSION);
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
		case VK_SHADER_STAGE_COMPUTE_BIT: return "Compute";
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
	template <size_t _KeySize>
	void GenerateKey(char(&key)[_KeySize], const ShaderFileDescription& vertexFileDesc, const ShaderFileDescription& fragFileDesc)
	{
		char vertexKey[_KeySize/2];
		shader_compiler::GenerateSpvFileName(vertexKey, vertexFileDesc.Filepath.c_str(), vertexFileDesc.CompileOptions);
		char fragKey[_KeySize/2];
		shader_compiler::GenerateSpvFileName(fragKey, fragFileDesc.Filepath.c_str(), fragFileDesc.CompileOptions);
		sprintf_s(key, "%s%s", vertexKey, fragKey);
		char* i = key;
		while (*i)
		{
			*i = tolower(*i);
			++i;
		}
	}

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

	bool ShaderCompiler::ProcessShaderFile(const char* filepath, VkShaderStageFlagBits shaderStage, const tCompileOptions& compileOptions)
	{
		PROFILE_SCOPE_LOG(ProcessShaderFile, "Shader file process");
		Logf(LogLevel::Info, "Compiling shader: [%s: %s]\n", vkutils::GetVulkanShaderStageName(shaderStage), filepath);

		check(!strcmp(compileOptions.EntryPointName, "main") && "Set shader entry point not supported yet.");

		// integrity check: dont repeat shader stage and ensure correct file extension with shader stage.
		check(GetCompiledModule(shaderStage) == VK_NULL_HANDLE);
		check(CheckShaderFileExtension(filepath, shaderStage));

		char binaryFilepath[1024];
		shader_compiler::GenerateSpvFileName(binaryFilepath, filepath, compileOptions);

#ifdef SHADER_RUNTIME_COMPILATION
		shader_compiler::tCompilationResult bin;
		if (
#ifndef SHADER_FORCE_COMPILATION
			GetSpvBinaryFromFile(filepath, binaryFilepath, &bin.binary, &bin.binaryCount)
#else
			false
#endif // !SHADER_FORCE_COMPILATION
			)
		{
			Logf(LogLevel::Info, "Loading shader binary from compiled file: %s.spv\n", filepath);
		}
		else
		{
			PROFILE_SCOPE_LOG(ShaderCompilation, "Compile shader");
			Logf(LogLevel::Warn, "Compiled binary not found for: %s\n", filepath);
			bin = shader_compiler::Compile(filepath, shaderStage, &compileOptions);
			check(bin.IsCompilationSucceed());
			GenerateCompiledFile(binaryFilepath, bin.binary, bin.binaryCount);
		}
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

		char buff[256];
		sprintf_s(buff, "ShaderModule_(%s)", binaryFilepath);
		SetVkObjectName(m_renderContext, &data.CompiledModule, VK_OBJECT_TYPE_SHADER_MODULE, buff);

		Logf(LogLevel::Ok, "Shader compiled successfully (%s: %s)\n", vkutils::GetVulkanShaderStageName(shaderStage), filepath);
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
		uint32_t size = (uint32_t)m_reflectionProperties.DescriptorSetInfoArray.size();
		m_cachedLayoutArray.resize(size);
		for (uint32_t i = 0; i < size; ++i)
		{
			check(m_reflectionProperties.DescriptorSetInfoArray[i].SetIndex < size);
			VkDescriptorSetLayout layout = GenerateDescriptorSetLayout(m_reflectionProperties.DescriptorSetInfoArray[i], layoutCache);
			m_cachedLayoutArray[m_reflectionProperties.DescriptorSetInfoArray[i].SetIndex] = layout;
		}
		for (const auto& it : m_reflectionProperties.PushConstantMap)
			m_cachedPushConstantArray.push_back(GeneratePushConstantInfo(it.second));

		return true;
	}

	void ShaderCompiler::SetUniformBufferAsDynamic(const char* uniformBufferName)
	{
		check(uniformBufferName && *uniformBufferName);
		for (uint32_t i = 0; i < (uint32_t)m_reflectionProperties.DescriptorSetInfoArray.size(); ++i)
		{
			for (uint32_t j = 0; j < (uint32_t)m_reflectionProperties.DescriptorSetInfoArray[i].BindingArray.size(); ++j)
			{
				ShaderBindingDescriptorInfo& bindingInfo = m_reflectionProperties.DescriptorSetInfoArray[i].BindingArray[j];
				if (!strcmp(bindingInfo.Name.c_str(), uniformBufferName))
				{
					switch (bindingInfo.Type)
					{
					case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: bindingInfo.Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; return;
					case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: bindingInfo.Type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC; return;
					default:
						Logf(LogLevel::Error, "Trying to set as uniform dynamic an invalid Descriptor [%s]\n", uniformBufferName);
					}
				}
			}
		}
		check(false);
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

				// spirv cross get naming heuristic for uniform buffers. 
				// (https://github.com/KhronosGroup/SPIRV-Cross/wiki/Reflection-API-user-guide#a-note-on-names)
				// case: layout (set, binding) uniform UBO {} u_ubo;
				// To get the name of the struct UBO use resource.name.c_str()
				// To get the name of the instance use compiler.get_name(resource.id)
				//bufferInfo.Name = resource.name.c_str();
				bufferInfo.Name = compiler.get_name(resource.id);
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

#ifdef MIST_SHADER_REFLECTION_LOG
				Logf(LogLevel::Debug, "> %s [ShaderStage: %s; Name:%s; Set: %u; Binding: %u; Size: %u; ArrayCount: %u;]\n", vkutils::GetVulkanDescriptorTypeName(descriptorType),
					vkutils::GetVulkanShaderStageName(shaderStage), bufferInfo.Name.c_str(), setIndex, bufferInfo.Binding, bufferInfo.Size, bufferInfo.ArrayCount);
#endif // MIST_SHADER_REFLECTION_LOG

				return setIndex;
			};

		check(binaryData && binaryCount && "Invalid binary source.");
		spirv_cross::CompilerGLSL compiler(binaryData, binaryCount);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

#ifdef MIST_SHADER_REFLECTION_LOG
		Log(LogLevel::Debug, "Processing shader reflection...\n");
#endif // MIST_SHADER_REFLECTION_LOG


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
#ifdef MIST_SHADER_REFLECTION_LOG
			Logf(LogLevel::Debug, "> PUSH_CONSTANT [ShaderStage: %s; Name: %s; Offset: %zd; Size: %zd]\n",
				vkutils::GetVulkanShaderStageName(shaderStage), info.Name.c_str(), info.Offset, info.Size);
#endif // MIST_SHADER_REFLECTION_LOG

		}
#ifdef MIST_SHADER_REFLECTION_LOG
		Log(LogLevel::Debug, "End shader reflection.\n");
#endif // MIST_SHADER_REFLECTION_LOG

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
			case VK_SHADER_STAGE_COMPUTE_BIT: desiredExt = SHADER_COMPUTE_FILE_EXTENSION; break;
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

	bool ShaderCompiler::GenerateCompiledFile(const char* shaderFilepath, uint32_t* binaryData, size_t binaryCount)
	{
		PROFILE_SCOPE_LOG(WriteShader, "Write file with binary spv shader");
		check(shaderFilepath && *shaderFilepath && binaryData && binaryCount);
		FILE* f;
		errno_t err = fopen_s(&f, shaderFilepath, "wb");
		check(!err && f && "Failed to write shader compiled file");
		size_t writtenCount = fwrite(binaryData, sizeof(uint32_t), binaryCount, f);
		check(writtenCount == binaryCount);
		fclose(f);
		Logf(LogLevel::Ok, "Shader binary compiled written: %s [%u bytes]\n", shaderFilepath, writtenCount * sizeof(uint32_t));
		return true;
	}

	bool ShaderCompiler::GetSpvBinaryFromFile(const char* shaderFilepath, const char* binaryFilepath, uint32_t** binaryData, size_t* binaryCount)
	{
		check(binaryFilepath && *binaryFilepath && binaryData && binaryCount);

		// Binary file is older than source file
		struct stat attrFile;
		stat(shaderFilepath, &attrFile);
		struct stat attrCompiled;
		stat(binaryFilepath, &attrCompiled);
		if (attrFile.st_mtime > attrCompiled.st_mtime)
			return false;

		PROFILE_SCOPE_LOG(SpvShader, "Read spv binary from file");
		// Binary file is created after last file modification, valid binary
		FILE* f;
		errno_t err = fopen_s(&f, binaryFilepath, "rb");
		check(!err && f && "Failed to open shader compiled file");
		fseek(f, 0L, SEEK_END);
		size_t numbytes = ftell(f);
		fseek(f, 0L, SEEK_SET);
		*binaryCount = numbytes / sizeof(uint32_t);
		*binaryData = (uint32_t*)malloc(numbytes);
		size_t contentRead = fread_s(*binaryData, numbytes, 1, numbytes, f);
		check(contentRead == numbytes);
		fclose(f);
		return true;
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
		tDynArray<VkDynamicState> DynamicStates;

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

		VkPipelineDynamicStateCreateInfo dynamicStateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .pNext = nullptr };
		dynamicStateInfo.flags = 0;
		dynamicStateInfo.pDynamicStates = DynamicStates.data();
		dynamicStateInfo.dynamicStateCount = (uint32_t)DynamicStates.size();

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
		if (DynamicStates.empty())
			pipelineInfo.pDynamicState = nullptr;
		else
			pipelineInfo.pDynamicState = &dynamicStateInfo;

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

	ShaderProgram* ShaderProgram::Create(const RenderContext& context, const GraphicsShaderProgramDescription& description)
	{
		ShaderProgram* program = new ShaderProgram();
		check(program->_Create(context, description));
		ShaderFileDB& db = *const_cast<ShaderFileDB*>(context.ShaderDB);
		db.AddShaderProgram(context, program);
		return program;
	}

	bool ShaderProgram::_Create(const RenderContext& context, const GraphicsShaderProgramDescription& description)
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
		m_pipeline = VK_NULL_HANDLE;
		m_pipelineLayout = VK_NULL_HANDLE;
	}

	bool ShaderProgram::Reload(const RenderContext& context)
	{
		check(m_pipeline == VK_NULL_HANDLE && m_pipelineLayout == VK_NULL_HANDLE);
		check(!m_description.VertexShaderFile.Filepath.empty() || !m_description.FragmentShaderFile.Filepath.empty());
		check(m_description.RenderTarget);
		RenderPipelineBuilder builder(context);
		// Input configuration
		builder.InputDescription = m_description.InputLayout;
		builder.SubpassIndex = m_description.SubpassIndex;
		builder.Topology = tovk::GetPrimitiveTopology(m_description.Topology);
		builder.Rasterizer.cullMode = tovk::GetCullMode(m_description.CullMode);
		builder.Rasterizer.frontFace = tovk::GetFrontFace(m_description.FrontFaceMode);
		builder.DepthStencil.depthWriteEnable = m_description.DepthStencilMode & DEPTH_STENCIL_DEPTH_WRITE ? VK_TRUE : VK_FALSE;
		builder.DepthStencil.depthTestEnable = m_description.DepthStencilMode & DEPTH_STENCIL_DEPTH_TEST ? VK_TRUE : VK_FALSE;
		builder.DepthStencil.depthBoundsTestEnable = m_description.DepthStencilMode & DEPTH_STENCIL_DEPTH_BOUNDS_TEST ? VK_TRUE : VK_FALSE;
		builder.DepthStencil.stencilTestEnable = m_description.DepthStencilMode & DEPTH_STENCIL_STENCIL_TEST ? VK_TRUE : VK_FALSE;
		builder.Viewport.width = (float)m_description.RenderTarget->GetWidth();
		builder.Viewport.height = (float)m_description.RenderTarget->GetHeight();
		builder.Scissor.extent = { m_description.RenderTarget->GetWidth(), m_description.RenderTarget->GetHeight() };
		builder.Scissor.offset = {0, 0};
		if (!m_description.DynamicStates.empty())
		{
			builder.DynamicStates.resize(m_description.DynamicStates.size());
			for (uint32_t i = 0; i < (uint32_t)m_description.DynamicStates.size(); ++i)
				builder.DynamicStates[i] = tovk::GetDynamicState(m_description.DynamicStates[i]);
		}

		// Generate shader module stages
		tArray<ShaderFileDescription*, 2> descs = { &m_description.VertexShaderFile, &m_description.FragmentShaderFile };
		tArray<VkShaderStageFlagBits, 2> stages = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };
		tArray<const char*, 2> entryPoints = { m_description.VertexShaderFile.CompileOptions.EntryPointName, m_description.FragmentShaderFile.CompileOptions.EntryPointName };
		ShaderCompiler compiler(context);
		for (uint32_t i = 0; i < (uint32_t)descs.size(); ++i)
		{
			if (!descs[i]->Filepath.empty())
			{
				compiler.ProcessShaderFile(descs[i]->Filepath.c_str(), stages[i], descs[i]->CompileOptions);
				VkShaderModule compiled = compiler.GetCompiledModule(stages[i]);
				builder.ShaderStages.push_back(vkinit::PipelineShaderStageCreateInfo(stages[i], compiled, entryPoints[i]));
			}
		}
		// Generate shader reflection data
		// Override with external info
		for (const tString& dynBuffer : m_description.DynamicBuffers)
			compiler.SetUniformBufferAsDynamic(dynBuffer.c_str());
		compiler.GenerateReflectionResources(*const_cast<DescriptorLayoutCache*>(context.LayoutCache));

		// Blending info for color attachments
		uint32_t colorAttachmentCount = m_description.RenderTarget->GetDescription().ColorAttachmentCount;
		check((uint32_t)m_description.ColorAttachmentBlendingArray.size() <= colorAttachmentCount);
		builder.ColorBlendAttachment.resize(colorAttachmentCount);
		for (uint32_t i = 0; i < colorAttachmentCount; ++i)
		{
			// Single blenc attachment without blending and writing RGBA
			if (i < (uint32_t)m_description.ColorAttachmentBlendingArray.size())
				builder.ColorBlendAttachment[i] = m_description.ColorAttachmentBlendingArray[i];
			else
				builder.ColorBlendAttachment[i] = vkinit::PipelineColorBlendAttachmentState();
		}

		// Pass layout info
		builder.LayoutInfo = vkinit::PipelineLayoutCreateInfo();
		builder.LayoutInfo.setLayoutCount = compiler.GetDescriptorSetLayoutCount();
		builder.LayoutInfo.pSetLayouts = compiler.GetDescriptorSetLayoutArray();
		if (m_description.PushConstantArray.empty())
		{
			builder.LayoutInfo.pushConstantRangeCount = (uint32_t)m_description.PushConstantArray.size();
			builder.LayoutInfo.pPushConstantRanges = m_description.PushConstantArray.data();
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

		char buff[2][1024];
		shader_compiler::GenerateSpvFileName(buff[0], m_description.VertexShaderFile.Filepath.c_str(), m_description.VertexShaderFile.CompileOptions);
		shader_compiler::GenerateSpvFileName(buff[1], m_description.FragmentShaderFile.Filepath.c_str(), m_description.FragmentShaderFile.CompileOptions);
		char vkName[2048];
		sprintf_s(vkName, "Pipeline_(%s)(%s)", buff[0], buff[1]);
		SetVkObjectName(context, &m_pipeline, VK_OBJECT_TYPE_PIPELINE, vkName);
		sprintf_s(vkName, "PipelineLayout_(%s)(%s)", buff[0], buff[1]);
		SetVkObjectName(context, &m_pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, vkName);

		return true;
	}

	void ShaderProgram::UseProgram(VkCommandBuffer cmd) const
	{
		RenderAPI::CmdBindGraphicsPipeline(cmd, m_pipeline);
	}

	void ShaderProgram::BindDescriptorSets(VkCommandBuffer cmd, const VkDescriptorSet* setArray, uint32_t setCount, uint32_t firstSet, const uint32_t* dynamicOffsetArray, uint32_t dynamicOffsetCount) const
	{
		RenderAPI::CmdBindGraphicsDescriptorSet(cmd, m_pipelineLayout, setArray, setCount, firstSet, dynamicOffsetArray, dynamicOffsetCount);
	}

	ComputeShader* ComputeShader::Create(const RenderContext& context, const ComputeShaderProgramDescription& description)
	{
		ComputeShader* shader = new ComputeShader();
		shader->m_description = description;
		shader->Reload(context);
		ShaderFileDB& db = *const_cast<ShaderFileDB*>(context.ShaderDB);
		db.AddShaderProgram(context, shader);
		return shader;
	}

	void ComputeShader::Destroy(const RenderContext& context)
	{
		vkDestroyPipeline(context.Device, m_pipeline, nullptr);
		vkDestroyPipelineLayout(context.Device, m_pipelineLayout, nullptr);
		m_pipeline = VK_NULL_HANDLE;
		m_pipelineLayout = VK_NULL_HANDLE;
	}

	bool ComputeShader::Reload(const RenderContext& context)
	{
		check(!m_description.ComputeShaderFile.Filepath.empty());
		check(m_pipeline == VK_NULL_HANDLE && m_pipelineLayout == VK_NULL_HANDLE);

		VkShaderStageFlagBits stage = VK_SHADER_STAGE_COMPUTE_BIT;
		ShaderCompiler compiler(context);
		compiler.ProcessShaderFile(m_description.ComputeShaderFile.Filepath.c_str(), stage, m_description.ComputeShaderFile.CompileOptions);

		for (uint32_t i = 0; i < (uint32_t)m_description.DynamicBuffers.size(); ++i)
			compiler.SetUniformBufferAsDynamic(m_description.DynamicBuffers[i].c_str());
		compiler.GenerateReflectionResources(*const_cast<DescriptorLayoutCache*>(context.LayoutCache));

		VkShaderModule module = compiler.GetCompiledModule(stage);
		check(module != VK_NULL_HANDLE);

		VkPipelineShaderStageCreateInfo shaderStageInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = nullptr };
		shaderStageInfo.module = module;
		shaderStageInfo.stage = stage;
		shaderStageInfo.pName = "main";
		shaderStageInfo.flags = 0;

		VkPipelineLayoutCreateInfo layoutInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .pNext = nullptr };
		layoutInfo.flags = 0;
		layoutInfo.setLayoutCount = compiler.GetDescriptorSetLayoutCount();
		layoutInfo.pSetLayouts = compiler.GetDescriptorSetLayoutArray();
		if (m_description.PushConstantArray.empty())
		{
			layoutInfo.pushConstantRangeCount = (uint32_t)m_description.PushConstantArray.size();
			layoutInfo.pPushConstantRanges = m_description.PushConstantArray.data();
		}
		else
		{
			layoutInfo.pushConstantRangeCount = compiler.GetPushConstantCount();
			layoutInfo.pPushConstantRanges = compiler.GetPushConstantArray();
		}

		vkcheck(vkCreatePipelineLayout(context.Device, &layoutInfo, nullptr, &m_pipelineLayout));
		check(m_pipelineLayout != VK_NULL_HANDLE);

		VkComputePipelineCreateInfo computeInfo{ .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .pNext = nullptr };
		computeInfo.layout = m_pipelineLayout;
		computeInfo.stage = shaderStageInfo;
		computeInfo.flags = 0;
		vkcheck(vkCreateComputePipelines(context.Device, nullptr, 1, &computeInfo, nullptr, &m_pipeline));
		check(m_pipeline != VK_NULL_HANDLE);
		return true;
	}

	void ComputeShader::UseProgram(CommandBuffer cmd) const
	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
		++Mist::Profiling::GRenderStats.ShaderProgramCount;
	}

	void ComputeShader::BindDescriptorSets(CommandBuffer cmd, const VkDescriptorSet* setArray, uint32_t setCount, uint32_t firstSet, const uint32_t* dynamicOffsetArray, uint32_t dynamicOffsetCount) const
	{
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, firstSet, setCount, setArray, dynamicOffsetCount, dynamicOffsetArray);
		++Mist::Profiling::GRenderStats.SetBindingCount;
	}


	void ShaderFileDB::AddShaderProgram(const RenderContext& context, ShaderProgram* program)
	{
		const GraphicsShaderProgramDescription& description = program->GetDescription();
		check(!FindShaderProgram(description.VertexShaderFile, description.FragmentShaderFile));

		uint32_t index = (uint32_t)m_shaderArray.size();
		m_shaderArray.push_back(program);
		char key[2048];
		GenerateKey(key, description.VertexShaderFile, description.FragmentShaderFile);
		m_indexMap[key] = index;
	}

	void ShaderFileDB::AddShaderProgram(const RenderContext& context, ComputeShader* computeShader)
	{
		const ComputeShaderProgramDescription& description = computeShader->GetDescription();
		check(!FindShaderProgram(description.ComputeShaderFile.Filepath.c_str()));
		uint32_t index = (uint32_t)m_computeShaderArray.size();
		m_computeShaderArray.push_back(computeShader);
		m_indexMap[description.ComputeShaderFile.Filepath.c_str()] = index;
	}

	ShaderProgram* ShaderFileDB::FindShaderProgram(const ShaderFileDescription& vertexFileDesc, const ShaderFileDescription& fragFileDesc) const
	{
		char key[2048];
		GenerateKey(key, vertexFileDesc, fragFileDesc);
		return FindShaderProgram(key);
	}

	ShaderProgram* ShaderFileDB::FindShaderProgram(const char* key) const
	{
		ShaderProgram* p = nullptr;
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
		for (uint32_t i = 0; i < (uint32_t)m_computeShaderArray.size(); ++i)
		{
			m_computeShaderArray[i]->Destroy(context);
			delete m_computeShaderArray[i];
			m_computeShaderArray[i] = nullptr;
		}
		m_computeShaderArray.clear();
		m_shaderArray.clear();
		m_indexMap.clear();
#ifdef SHADER_RUNTIME_COMPILATION
#endif // SHADER_RUNTIME_COMPILATION
	}
}
