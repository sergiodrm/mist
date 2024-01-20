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



	struct ShaderBindingDescriptorInfo
	{
		VkDescriptorType Type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		uint32_t Size = 0;
		uint32_t Binding = 0;
		uint32_t ArrayCount = 0;
		std::string Name;
	};

	struct ShaderDescriptorSetInfo
	{
		VkShaderStageFlagBits Stage{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
		std::vector<ShaderBindingDescriptorInfo> BindingArray;
		uint32_t SetIndex = 0;
	};

	struct ShaderPushConstantBufferInfo
	{
		std::string Name;
		uint32_t Offset = 0;
		uint32_t Size = 0;
		VkShaderStageFlagBits ShaderStage;
	};

	struct ShaderReflectionProperties
	{
		std::unordered_map<VkShaderStageFlagBits, std::vector<ShaderDescriptorSetInfo>> DescriptorSetMap;
		std::unordered_map<VkShaderStageFlagBits, ShaderPushConstantBufferInfo> PushConstantMap;
	};

	class ShaderCompiler
	{
		struct CachedBinaryData
		{
			VkShaderStageFlagBits ShaderStage;
			std::vector<uint32_t> BinarySource;
			VkShaderModule CompiledModule;
		};
	public:
		ShaderCompiler(const RenderContext& renderContext);
		~ShaderCompiler();

		void ClearCachedData();
		bool ProcessShaderFile(const char* filepath, VkShaderStageFlagBits shaderStage);
		VkShaderModule GetCompiledModule(VkShaderStageFlagBits shaderStage) const;
		VkPipelineLayoutCreateInfo GeneratePipelineLayoutCreateInfo();

		/**
		 * Create VkShaderModule from a cached source file.
		 * This will compile the shader code in the file.
		 * @return VkShaderModule valid if the compilation terminated successfully. VK_NULL_HANDLE otherwise.
		 */
		VkShaderModule Compile(const std::vector<uint32_t>& binarySource, VkShaderStageFlagBits flag) const;

		static bool CacheSourceFromFile(const char* file, std::vector<uint32_t>& outCachedData);
		static bool CheckShaderFileExtension(const char* filepath, VkShaderStageFlagBits shaderStage);
	protected:
		void ProcessCachedSource(CachedBinaryData& cachedData);
		ShaderDescriptorSetInfo& FindOrCreateDescriptorSet(uint32_t setIndex, VkShaderStageFlagBits shaderStage);
		const ShaderDescriptorSetInfo& GetDescriptorSet(uint32_t setIndex, VkShaderStageFlagBits shaderStage) const;
		VkDescriptorSetLayout GenerateDescriptorSetLayout(const ShaderDescriptorSetInfo& setInfo) const;
		VkPushConstantRange GeneratePushConstantInfo(const ShaderPushConstantBufferInfo& pushConstantInfo) const;

	private:
		const RenderContext& m_renderContext;
		std::unordered_map<VkShaderStageFlagBits, CachedBinaryData> m_cachedBinarySources;
		// Keep generated layouts in memory, we need to release them at any moment. 
		std::vector<VkDescriptorSetLayout> m_cachedLayoutArray;
		std::vector<VkPushConstantRange> m_cachedPushConstantArray;
		ShaderReflectionProperties m_reflectionProperties;
	};

	struct ShaderBufferCreateInfo
	{
		RenderContext RContext;
		ShaderBindingDescriptorInfo Resource;
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
		ShaderBindingDescriptorInfo m_resource;
		AllocatedBuffer m_buffer;
	};
}