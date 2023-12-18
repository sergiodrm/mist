// src file for vkmmc project 

#include "Shader.h"

#include <spirv-cross-main/spirv_glsl.hpp>

#include "RenderTypes.h"
#include "GenericUtils.h"
#include "Debug.h"

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
		VkShaderStageFlagBits flags[] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };
		const uint32_t count = sizeof(flags) / sizeof(VkShaderStageFlagBits);
		for (uint32_t i = 0; i < count; ++i)
		{
			ProcessCachedSource(m_cachedBinarySources[flags[i]]);
		}
		return true;
	}

	bool ShaderCompiler::CacheSourceFromFile(const char* file, std::vector<uint32_t>& outCachedData)
	{
		outCachedData.clear();
		return vkmmc_utils::ReadFile(file, outCachedData);
	}

	void ShaderCompiler::ProcessCachedSource(const std::vector<uint32_t>& cachedSource)
	{
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
			sampler.ArraySize = compiler.get_decoration(resource.id, spv::DecorationArrayStride);
			sampler.Name = resource.name.c_str();
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
}
