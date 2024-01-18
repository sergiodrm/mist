// src file for vkmmc project 

#include "Shader.h"

#include <spirv-cross-main/spirv_glsl.hpp>

#include "RenderTypes.h"
#include "GenericUtils.h"
#include "Debug.h"
#include "InitVulkanTypes.h"
#include <set>

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

	const char* GetVulkanShaderStageName(VkShaderStageFlagBits shaderStage)
	{
		switch (shaderStage)
		{
		case VK_SHADER_STAGE_VERTEX_BIT: return "Vertex";
		case VK_SHADER_STAGE_FRAGMENT_BIT: return "Fragment";
		}
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

namespace vkmmc
{
	ShaderCompiler::ShaderCompiler(const RenderContext& renderContext) : m_renderContext(renderContext)
	{}

	ShaderCompiler::~ShaderCompiler()
	{
		ClearCachedData();
	}

	void ShaderCompiler::ClearCachedData()
	{
		for (auto it : m_cachedBinarySources)
		{
			vkDestroyShaderModule(m_renderContext.Device, it.second.CompiledModule, nullptr);
		}
		m_cachedBinarySources.clear();

		// TODO: descriptor layout destruction. The ownership of the layouts will be moved to ShaderProgram when be ready.
		for (auto& it : m_cachedLayouts)
		{
			for (uint32_t i = 0; i < it.second.size(); ++i)
			{
				vkDestroyDescriptorSetLayout(m_renderContext.Device, it.second[i], nullptr);
			}
			it.second.clear();
		}
		m_cachedLayouts.clear();
	}

	bool ShaderCompiler::ProcessShaderFile(const char* filepath, VkShaderStageFlagBits shaderStage)
	{
		Logf(LogLevel::Ok, "Compiling shader: [%s: %s]\n", vkutils::GetVulkanShaderStageName(shaderStage), filepath);
		check(!m_cachedBinarySources.contains(shaderStage));
		CachedBinaryData data;
		data.ShaderStage = shaderStage;
		check(CheckShaderFileExtension(filepath, shaderStage));
		check(CacheSourceFromFile(filepath, data.BinarySource));
		ProcessCachedSource(data);
		data.CompiledModule = Compile(data.BinarySource, data.ShaderStage);
		m_cachedBinarySources[shaderStage] = data;
		Logf(LogLevel::Ok, "Shader compiled and cached successfully: [%s: %s]\n", vkutils::GetVulkanShaderStageName(shaderStage), filepath);
		return true;
	}

	VkShaderModule ShaderCompiler::GetCompiledModule(VkShaderStageFlagBits shaderStage) const
	{
		return m_cachedBinarySources.contains(shaderStage) ? m_cachedBinarySources.at(shaderStage).CompiledModule : VK_NULL_HANDLE;
	}

	VkDescriptorSetLayout ShaderCompiler::GetDescriptorSetLayout(VkShaderStageFlagBits shaderStage, uint32_t setIndex) const
	{
		return m_cachedBinarySources.contains(shaderStage) ? m_cachedBinarySources.at(shaderStage).SetLayouts[setIndex] : VK_NULL_HANDLE;
	}

	uint32_t ShaderCompiler::GetDescriptorSetLayoutCount(VkShaderStageFlagBits shaderStage) const
	{
		return m_cachedBinarySources.contains(shaderStage) ? (uint32_t)m_cachedBinarySources.at(shaderStage).SetLayouts.size() : 0;
	}

	VkDescriptorSetLayout ShaderCompiler::GenerateDescriptorSetLayout(uint32_t setIndex, VkShaderStageFlagBits shaderStage)
	{
		VkDescriptorSetLayout setLayout{VK_NULL_HANDLE};
		if (m_cachedLayouts.contains(shaderStage) && setIndex < m_cachedLayouts[shaderStage].size() && m_cachedLayouts[shaderStage][setIndex] != VK_NULL_HANDLE)
		{
			setLayout = m_cachedLayouts[shaderStage][setIndex];
		}
		else
		{
			const ShaderDescriptorSetInfo& setInfo = GetDescriptorSet(setIndex, shaderStage);
			std::vector<VkDescriptorSetLayoutBinding> bindingArray(setInfo.BindingArray.size());
			for (uint32_t i = 0; i < setInfo.BindingArray.size(); ++i)
			{
				bindingArray[i].binding = i;
				bindingArray[i].descriptorCount = setInfo.BindingArray[i].ArrayCount;
				bindingArray[i].descriptorType = setInfo.BindingArray[i].Type;
				bindingArray[i].pImmutableSamplers = nullptr;
				bindingArray[i].stageFlags = shaderStage;
			}
			VkDescriptorSetLayoutCreateInfo createInfo = vkinit::DescriptorSetLayoutCreateInfo(bindingArray.data(), (uint32_t)bindingArray.size());
			vkcheck(vkCreateDescriptorSetLayout(m_renderContext.Device, &createInfo, nullptr, &setLayout));
			if (!m_cachedLayouts.contains(shaderStage))
				m_cachedLayouts[shaderStage] = {};
			if (m_cachedLayouts[shaderStage].size() < setIndex + 1)
				m_cachedLayouts[shaderStage].resize(setIndex + 1, VK_NULL_HANDLE);
			check(m_cachedLayouts[shaderStage][setIndex] == VK_NULL_HANDLE);
			m_cachedLayouts[shaderStage][setIndex] = setLayout;
		}
		return setLayout;
	}

	VkShaderModule ShaderCompiler::Compile(const std::vector<uint32_t>& binarySource, VkShaderStageFlagBits flag) const
	{
		check(!binarySource.empty());
		VkShaderModuleCreateInfo info{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .pNext = nullptr };
		info.codeSize = binarySource.size() * sizeof(uint32_t);
		info.pCode = binarySource.data();
		VkShaderModule shaderModule{ VK_NULL_HANDLE };
		vkcheck(vkCreateShaderModule(m_renderContext.Device, &info, nullptr, &shaderModule));
		return shaderModule;
	}

	bool ShaderCompiler::CacheSourceFromFile(const char* file, std::vector<uint32_t>& outCachedData)
	{
		outCachedData.clear();
		return vkmmc_utils::ReadFile(file, outCachedData);
	}

	void ShaderCompiler::ProcessCachedSource(CachedBinaryData& cachedData)
	{
		std::set<uint32_t> setIndexProcessed;;
		auto processSpirvResource = [this](const spirv_cross::CompilerGLSL& compiler, const spirv_cross::Resource& resource, VkShaderStageFlagBits shaderStage, VkDescriptorType descriptorType)
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
				uint32_t setIndex = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);

				ShaderDescriptorSetInfo& descriptorSetInfo = FindOrCreateDescriptorSet(setIndex, shaderStage);
				if (descriptorSetInfo.BindingArray.size() < bufferInfo.Binding + 1)
					descriptorSetInfo.BindingArray.resize(bufferInfo.Binding + 1);
				descriptorSetInfo.BindingArray[bufferInfo.Binding] = bufferInfo;

				Logf(LogLevel::Debug, "> %s [ShaderStage: %s; Name:%s; Set: %u; Binding: %u; Size: %u; ArrayCount: %u;]\n", vkutils::GetVulkanDescriptorTypeName(descriptorType),
					vkutils::GetVulkanShaderStageName(shaderStage), bufferInfo.Name.c_str(), setIndex, bufferInfo.Binding, bufferInfo.Size, bufferInfo.ArrayCount);
				return setIndex;
			};

		const std::vector<uint32_t>& cachedSource = cachedData.BinarySource;
		check(!cachedSource.empty() && "Invalid cached source.");
		spirv_cross::CompilerGLSL compiler(cachedSource);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		Log(LogLevel::Debug, "Processing shader reflection...\n");
		
		for (const auto& resource : resources.uniform_buffers)
			setIndexProcessed.insert(processSpirvResource(compiler, resource, cachedData.ShaderStage, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER));

		for (const auto& resource : resources.storage_buffers)
			setIndexProcessed.insert(processSpirvResource(compiler, resource, cachedData.ShaderStage, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));

		for (const auto& resource : resources.sampled_images)
			setIndexProcessed.insert(processSpirvResource(compiler, resource, cachedData.ShaderStage, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER));
		Log(LogLevel::Debug, "End shader reflection.\n");

		for (uint32_t setIndex : setIndexProcessed)
		{
			GenerateDescriptorSetLayout(setIndex, cachedData.ShaderStage);
		}
	}

	ShaderDescriptorSetInfo& ShaderCompiler::FindOrCreateDescriptorSet(uint32_t setIndex, VkShaderStageFlagBits shaderStage)
	{
		if (!m_reflectionProperties.DescriptorSetMap.contains(shaderStage))
			m_reflectionProperties.DescriptorSetMap[shaderStage] = {};
		std::vector<ShaderDescriptorSetInfo>& descriptorSetArray = m_reflectionProperties.DescriptorSetMap[shaderStage];
		if (descriptorSetArray.size() < setIndex + 1)
			descriptorSetArray.resize(setIndex + 1);
		descriptorSetArray[setIndex].SetIndex = setIndex;
		descriptorSetArray[setIndex].Stage = shaderStage;
		return descriptorSetArray[setIndex];
	}

	const ShaderDescriptorSetInfo& ShaderCompiler::GetDescriptorSet(uint32_t setIndex, VkShaderStageFlagBits shaderStage) const
	{
		check(m_reflectionProperties.DescriptorSetMap.contains(shaderStage));
		check(setIndex < (uint32_t)m_reflectionProperties.DescriptorSetMap.at(shaderStage).size());
		return m_reflectionProperties.DescriptorSetMap.at(shaderStage)[setIndex];
	}

	bool ShaderCompiler::CheckShaderFileExtension(const char* filepath, VkShaderStageFlagBits shaderStage)
	{
		bool res = false;
		if (filepath && *filepath)
		{
			const char* desiredExt;
			switch (shaderStage)
			{
			case VK_SHADER_STAGE_FRAGMENT_BIT: desiredExt = ".frag.spv"; break;
			case VK_SHADER_STAGE_VERTEX_BIT: desiredExt = ".vert.spv"; break;
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

	void ShaderBuffer::Init(const ShaderBufferCreateInfo& info)
	{
		check(info.Resource.Type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

		uint32_t size = info.Resource.Type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ?
			info.StorageSize : info.Resource.Size;
		m_buffer = Memory::CreateBuffer(info.RContext.Allocator,
			size, vkutils::GetVulkanBufferUsage(info.Resource.Type),
			VMA_MEMORY_USAGE_CPU_TO_GPU);
		m_resource = info.Resource;
		m_resource.Size = size;
	}

	void ShaderBuffer::Destroy(const RenderContext& renderContext)
	{
		Memory::DestroyBuffer(renderContext.Allocator, m_buffer);
	}

	void ShaderBuffer::SetBoundDescriptorSet(const RenderContext& renderContext, VkDescriptorSet newDescriptorSet)
	{
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = m_buffer.Buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = m_resource.Size;
		VkWriteDescriptorSet writeInfo{};
		writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo.pNext = nullptr;
		writeInfo.dstBinding = m_resource.Binding;
		writeInfo.dstArrayElement = 0;
		writeInfo.descriptorType = m_resource.Type;
		writeInfo.descriptorCount = 1;
		writeInfo.dstSet = newDescriptorSet;
		writeInfo.pBufferInfo = &bufferInfo;
		vkUpdateDescriptorSets(renderContext.Device, 1, &writeInfo, 0, nullptr);
	}

	void ShaderBuffer::CopyData(const RenderContext& renderContext, const void* source, uint32_t size)
	{
		check(size <= m_resource.Size);
		Memory::MemCopyDataToBuffer(renderContext.Allocator, m_buffer.Alloc, source, size);
	}
}
