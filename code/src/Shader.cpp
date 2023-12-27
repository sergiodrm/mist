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

	ShaderCompiler::~ShaderCompiler()
	{
		m_cachedBinarySources.clear();
	}

	ShaderCompiler::ShaderCompiler(const ShaderModuleLoadDescription* shaders, uint32_t count)
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			if (CacheSourceFromFile(shaders[i].ShaderFilePath.c_str(), m_cachedBinarySources[shaders[i].Flags]))
				Logf(LogLevel::Ok, "Shader loaded successfully [%s;%zd bytes].\n", 
					shaders[i].ShaderFilePath.c_str(), m_cachedBinarySources[shaders[i].Flags].size() * sizeof(uint32_t));
			else
				Logf(LogLevel::Error, "Failed to load shader [%s].\n", shaders[i].ShaderFilePath.c_str());
		}
	}

	VkShaderModule ShaderCompiler::Compile(const RenderContext& renderContext, VkShaderStageFlagBits flag)
	{
		std::vector<uint32_t>& cachedSource = m_cachedBinarySources[flag];
		check(!cachedSource.empty() && "Invalid cached data.");
		Logf(LogLevel::Debug, "Compiling cached shader...\n");
		// Create shader
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.codeSize = cachedSource.size() * sizeof(uint32_t);
		createInfo.pCode = cachedSource.data();

		// If fail, shader compilation failed...
		VkShaderModule shaderModule;
		vkcheck(vkCreateShaderModule(renderContext.Device, &createInfo, nullptr, &shaderModule));
		Logf(LogLevel::Ok, "Shader compilation completed.\n");
		return shaderModule;
	}

	void ShaderCompiler::Compile(const RenderContext& renderContext, std::vector<VkShaderModule>& outModules)
	{
		outModules.clear();
		outModules.resize(m_cachedBinarySources.size());
		uint32_t counter = 0;
		for (const std::pair<VkShaderStageFlagBits, std::vector<uint32_t>>& it : m_cachedBinarySources)
			outModules[counter++] = Compile(renderContext, it.first);
	}

	bool ShaderCompiler::ProcessReflectionProperties()
	{
		for (const std::pair<VkShaderStageFlagBits, std::vector<uint32_t>>& it : m_cachedBinarySources)
		{
			ProcessCachedSource(it.first);
		}
		return true;
	}

	bool ShaderCompiler::CacheSourceFromFile(const char* file, std::vector<uint32_t>& outCachedData)
	{
		outCachedData.clear();
		return vkmmc_utils::ReadFile(file, outCachedData);
	}

	void ShaderCompiler::ProcessCachedSource(VkShaderStageFlagBits flag)
	{
		const std::vector<uint32_t>& cachedSource = m_cachedBinarySources[flag];
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
			bufferInfo.Stage = flag;
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
			bufferInfo.Stage = flag;
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
			sampler.Stage = flag;
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
