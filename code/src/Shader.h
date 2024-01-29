// header file for vkmmc project 
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>

namespace vkmmc
{
	struct RenderContext;
	class DescriptorLayoutCache;

	struct ShaderBindingDescriptorInfo
	{
		VkDescriptorType Type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		uint32_t Size = 0;
		uint32_t Binding = 0;
		uint32_t ArrayCount = 0;
		VkShaderStageFlagBits Stage{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
		std::string Name;
	};

	struct ShaderDescriptorSetInfo
	{
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
		std::vector<ShaderDescriptorSetInfo> DescriptorSetInfoArray;
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
		bool GenerateResources(DescriptorLayoutCache& layoutCache);
		VkShaderModule GetCompiledModule(VkShaderStageFlagBits shaderStage) const;
		
		// Obtain generated resources. These functions will return nullptr/0 until call GenerateResources().
		const VkDescriptorSetLayout* GetDescriptorSetLayoutArray() const { return m_cachedLayoutArray.data(); };
		uint32_t GetDescriptorSetLayoutCount() const { return (uint32_t)m_cachedLayoutArray.size(); }
		const VkPushConstantRange* GetPushConstantArray() const { return m_cachedPushConstantArray.data(); };
		uint32_t GetPushConstantCount() const { return (uint32_t)m_cachedPushConstantArray.size(); }

		const ShaderReflectionProperties& GetReflectionProperties() const { return m_reflectionProperties; }


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
		ShaderDescriptorSetInfo& FindOrCreateDescriptorSet(uint32_t set);
		const ShaderDescriptorSetInfo& GetDescriptorSet(uint32_t set) const;
		VkDescriptorSetLayout GenerateDescriptorSetLayout(const ShaderDescriptorSetInfo& setInfo, DescriptorLayoutCache& layoutCache) const;
		VkPushConstantRange GeneratePushConstantInfo(const ShaderPushConstantBufferInfo& pushConstantInfo) const;

	private:
		const RenderContext& m_renderContext;
		std::unordered_map<VkShaderStageFlagBits, CachedBinaryData> m_cachedBinarySources;
		std::vector<VkDescriptorSetLayout> m_cachedLayoutArray;
		std::vector<VkPushConstantRange> m_cachedPushConstantArray;
		ShaderReflectionProperties m_reflectionProperties;
	};
}