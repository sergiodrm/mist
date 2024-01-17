// src file for vkmmc project 

#include "Shader.h"

#include <spirv-cross-main/spirv_glsl.hpp>

#include "RenderTypes.h"
#include "GenericUtils.h"
#include "Debug.h"

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
	}

	bool ShaderCompiler::ProcessShaderFile(const char* filepath, VkShaderStageFlagBits shaderStage)
	{
		check(!m_cachedBinarySources.contains(shaderStage));
		CachedBinaryData data;
		data.ShaderStage = shaderStage;
		check(CheckShaderFileExtension(filepath, shaderStage));
		check(CacheSourceFromFile(filepath, data.BinarySource));
		ProcessCachedSource(data);
		data.CompiledModule = Compile(data.BinarySource, data.ShaderStage);
		m_cachedBinarySources[shaderStage] = data;
		Logf(LogLevel::Ok, "Shader compiled and cached successfully: [%s]", filepath);
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
		const std::vector<uint32_t>& cachedSource = cachedData.BinarySource;
		check(!cachedSource.empty() && "Invalid cached source.");
		spirv_cross::CompilerGLSL compiler(cachedSource);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		Log(LogLevel::Debug, "Shader reflection properties dump:\n");
		for (const auto& resource : resources.uniform_buffers)
		{
			ShaderUniformBuffer bufferInfo;
			const auto& bufferType = compiler.get_type(resource.base_type_id);
			bufferInfo.DescriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			bufferInfo.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			bufferInfo.Name = resource.name.c_str();
			bufferInfo.Size = (uint32_t)compiler.get_declared_struct_size(bufferType);
			bufferInfo.Stage = cachedData.ShaderStage;
			Logf(LogLevel::Debug, "> Uniform buffer [Set: %u; Binding: %u; Size: %u; Name:%s]\n",
				bufferInfo.DescriptorSet, bufferInfo.Binding, bufferInfo.Size, bufferInfo.Name.c_str());

			ShaderDescriptorSet& descriptor = GetDescriptorSet(bufferInfo.DescriptorSet);
			if (descriptor.UniformBuffers.contains(bufferInfo.Binding))
				Logf(LogLevel::Error, "Overriding uniform buffer at location (set: %u, binding: %u) [Name: %s].\n",
					bufferInfo.DescriptorSet, bufferInfo.Binding, bufferInfo.Name.c_str());
			descriptor.UniformBuffers[bufferInfo.Binding] = bufferInfo;
		}

		for (const auto& resource : resources.storage_buffers)
		{
			ShaderStorageBuffer bufferInfo;
			const auto& bufferType = compiler.get_type(resource.base_type_id);
			bufferInfo.DescriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			bufferInfo.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			bufferInfo.Name = resource.name.c_str();
			bufferInfo.Size = (uint32_t)compiler.get_declared_struct_size(bufferType);
			bufferInfo.Stage = cachedData.ShaderStage;
			Logf(LogLevel::Debug, "> Storage buffer [Set: %u; Binding: %u; Size: %u; Name:%s]\n",
				bufferInfo.DescriptorSet, bufferInfo.Binding, bufferInfo.Size, bufferInfo.Name.c_str());

			ShaderDescriptorSet& descriptor = GetDescriptorSet(bufferInfo.DescriptorSet);
			if (descriptor.StorageBuffers.contains(bufferInfo.Binding))
				Logf(LogLevel::Error, "Overriding storage buffer at location (set: %u, binding: %u) [Name: %s].\n",
					bufferInfo.DescriptorSet, bufferInfo.Binding, bufferInfo.Name.c_str());
			descriptor.StorageBuffers[bufferInfo.Binding] = bufferInfo;
		}

		for (const auto& resource : resources.sampled_images)
		{
			ShaderImageSampler sampler;
			sampler.DescriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			sampler.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			sampler.Size = compiler.get_decoration(resource.id, spv::DecorationArrayStride);
			sampler.Name = resource.name.c_str();
			sampler.Stage = cachedData.ShaderStage;
			Logf(LogLevel::Debug, "> Sampler image [Set: %zd; Binding: %zd; Name:%s]\n",
				sampler.DescriptorSet, sampler.Binding, sampler.Name.c_str());

			ShaderDescriptorSet& descriptor = GetDescriptorSet(sampler.DescriptorSet);
			if (descriptor.ImageSamplers.contains(sampler.Binding))
				Logf(LogLevel::Error, "Overriding uniform buffer at location (set: %zd, binding: %zd) [Name: %s].\n",
					sampler.DescriptorSet, sampler.Binding, sampler.Name.c_str());
			descriptor.ImageSamplers[sampler.Binding] = sampler;
		}
	}

	ShaderDescriptorSet& ShaderCompiler::GetDescriptorSet(uint32_t setIndex)
	{
		if (m_reflectionProperties.DescriptorSetArray.size() < setIndex + 1)
			m_reflectionProperties.DescriptorSetArray.resize(setIndex + 1);
		return m_reflectionProperties.DescriptorSetArray[setIndex];
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
