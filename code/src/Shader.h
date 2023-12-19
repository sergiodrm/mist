// header file for vkmmc project 
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>
#include "RenderTypes.h"

namespace vkmmc
{
	struct RenderContext;

	struct ShaderResource
	{
		VkDescriptorType Type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		uint32_t DescriptorSet = 0;
		uint32_t Size = 0;
		uint32_t Binding = 0;
		std::string Name;
		VkShaderStageFlagBits Stage{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
	};

	struct ShaderUniformBuffer : public ShaderResource
	{
		ShaderUniformBuffer() { Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; }
	};

	struct ShaderStorageBuffer : public ShaderResource
	{
		ShaderStorageBuffer() { Type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; }
	};

	struct ShaderImageSampler : public ShaderResource
	{
		ShaderImageSampler() { Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; }
	};

	struct ShaderDescriptorSet
	{
		// Mapped by binding index
		std::unordered_map<uint32_t, ShaderUniformBuffer> UniformBuffers;
		std::unordered_map<uint32_t, ShaderStorageBuffer> StorageBuffers;
		std::unordered_map<uint32_t, ShaderImageSampler> ImageSamplers;
	};

	struct ShaderReflectionProperties
	{
		std::vector<ShaderDescriptorSet> DescriptorSetArray;
	};

	struct ShaderModuleLoadDescription
	{
		std::string ShaderFilePath;
		VkShaderStageFlagBits Flags;
	};

	class ShaderCompiler
	{
	public:
		ShaderCompiler(const ShaderModuleLoadDescription* shadersDescription, uint32_t count);
		~ShaderCompiler();

		/**
		 * Create VkShaderModule from a cached source file.
		 * This will compile the shader code in the file.
		 * @return VkShaderModule valid if the compilation terminated successfully. VK_NULL_HANDLE otherwise.
		 */
		VkShaderModule Compile(const RenderContext& renderContext, VkShaderStageFlagBits flag);
		void Compile(const RenderContext& renderContext, std::vector<VkShaderModule>& outModules);
		/**
		 * Fill reflection properties with cached source file.
		 * @return true if processed successfully. false otherwise.
		 */
		bool ProcessReflectionProperties();
		const ShaderReflectionProperties& GetReflectionProperties() const { return m_reflectionProperties; }

		static bool CacheSourceFromFile(const char* file, std::vector<uint32_t>& outCachedData);
	protected:
		void ProcessCachedSource(VkShaderStageFlagBits flag);
		ShaderDescriptorSet& GetDescriptorSet(uint32_t setIndex);

	private:
		std::unordered_map<VkShaderStageFlagBits, std::vector<uint32_t>> m_cachedBinarySources;
		ShaderReflectionProperties m_reflectionProperties;
	};

	struct ShaderBufferCreateInfo
	{
		RenderContext RContext;
		ShaderResource Resource;
		uint32_t StorageSize = 0;
		VkDescriptorSet DescriptorSet;
	};

	class ShaderBuffer
	{
	public:
		void Init(const ShaderBufferCreateInfo& info);
		void Destroy(const RenderContext& renderContext);
		const char* GetName() const { return m_resource.Name.c_str(); }
		void SetBoundDescriptorSet(const RenderContext& renderContext, VkDescriptorSet newDescriptorSet);
		void CopyData(const RenderContext& renderContext, const void* source, uint32_t size);
	private:
		ShaderResource m_resource;
		AllocatedBuffer m_buffer;
	};
}