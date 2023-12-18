// header file for vkmmc project 
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>

namespace vkmmc
{
	struct RenderContext;

	struct ShaderUniformBuffer
	{
		uint32_t DescriptorSet = 0;
		uint32_t Size = 0;
		uint32_t Binding = 0;
		std::string Name;
		VkShaderStageFlagBits Stage{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
	};

	struct ShaderStorageBuffer
	{
		uint32_t DescriptorSet = 0;
		uint32_t Size = 0;
		uint32_t Binding = 0;
		std::string Name;
		VkShaderStageFlagBits Stage{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
	};

	struct ShaderImageSampler
	{
		uint32_t Binding = 0;
		uint32_t DescriptorSet = 0;
		uint32_t ArraySize = 0;
		std::string Name;
		VkShaderStageFlagBits Stage{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
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
		void ProcessCachedSource(const std::vector<uint32_t>& cachedSource);
		ShaderDescriptorSet& GetDescriptorSet(uint32_t setIndex);

	private:
		std::unordered_map<VkShaderStageFlagBits, std::vector<uint32_t>> m_cachedBinarySources;
		ShaderReflectionProperties m_reflectionProperties;
	};
}