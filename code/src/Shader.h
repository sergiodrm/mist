// header file for Mist project 
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>
#include "RenderTypes.h"
#include "VulkanBuffer.h"

namespace Mist
{
	// Forward declarations
	struct RenderContext;
	class DescriptorLayoutCache;
	class RenderPipeline;
	class RenderTarget;


	struct ShaderBindingDescriptorInfo
	{
		VkDescriptorType Type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		uint32_t Size = 0;
		uint32_t Binding = 0;
		uint32_t ArrayCount = 0;
		VkShaderStageFlags Stage{ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
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
		VkShaderStageFlags ShaderStage;
	};

	struct ShaderReflectionProperties
	{
		std::vector<ShaderDescriptorSetInfo> DescriptorSetInfoArray;
		std::unordered_map<VkShaderStageFlags, ShaderPushConstantBufferInfo> PushConstantMap;
	};

	class ShaderCompiler
	{
		struct CompiledShaderModule
		{
			VkShaderStageFlags ShaderStage;
			VkShaderModule CompiledModule;
		};
	public:
		ShaderCompiler(const RenderContext& renderContext);
		~ShaderCompiler();

		void ClearCachedData();
		bool ProcessShaderFile(const char* filepath, VkShaderStageFlagBits shaderStage, bool forceCompilation = false);
		bool GenerateReflectionResources(DescriptorLayoutCache& layoutCache);
		void SetUniformBufferAsDynamic(const char* uniformBufferName);
		VkShaderModule GetCompiledModule(VkShaderStageFlags shaderStage) const;
		
		// Obtain generated resources. These functions will return nullptr/0 until call GenerateResources().
		const VkDescriptorSetLayout* GetDescriptorSetLayoutArray() const { return m_cachedLayoutArray.data(); };
		uint32_t GetDescriptorSetLayoutCount() const { return (uint32_t)m_cachedLayoutArray.size(); }
		const VkPushConstantRange* GetPushConstantArray() const { return m_cachedPushConstantArray.data(); };
		uint32_t GetPushConstantCount() const { return (uint32_t)m_cachedPushConstantArray.size(); }

		const ShaderReflectionProperties& GetReflectionProperties() const { return m_reflectionProperties; }


		/**
		 * Create VkShaderModule from a cached source file.
		 * This will compile the shader code in the file.
		 * @binarySize bytes count.
		 * @return VkShaderModule valid if the compilation terminated successfully. VK_NULL_HANDLE otherwise.
		 */
		static VkShaderModule CompileShaderModule(const RenderContext& context, const uint32_t* binaryData, size_t binaryCount, VkShaderStageFlags stage);
		static bool CheckShaderFileExtension(const char* filepath, VkShaderStageFlags shaderStage);
		static bool GenerateCompiledFile(const char* shaderFilepath, uint32_t* binaryData, size_t binaryCount);
		static bool GetSpvBinaryFromFile(const char* shaderFilepath, uint32_t** binaryData, size_t* binaryCount);
	protected:
		void ProcessReflection(VkShaderStageFlags shaderStage, uint32_t* binaryData, size_t binaryCount);
		
		ShaderDescriptorSetInfo& FindOrCreateDescriptorSet(uint32_t set);
		const ShaderDescriptorSetInfo& GetDescriptorSet(uint32_t set) const;
		VkDescriptorSetLayout GenerateDescriptorSetLayout(const ShaderDescriptorSetInfo& setInfo, DescriptorLayoutCache& layoutCache) const;
		VkPushConstantRange GeneratePushConstantInfo(const ShaderPushConstantBufferInfo& pushConstantInfo) const;

	private:
		const RenderContext& m_renderContext;
		tDynArray<CompiledShaderModule> m_modules;
		tDynArray<VkDescriptorSetLayout> m_cachedLayoutArray;
		tDynArray<VkPushConstantRange> m_cachedPushConstantArray;
		ShaderReflectionProperties m_reflectionProperties;
	};

	struct ShaderDescription
	{
		std::string Filepath;
		VkShaderStageFlagBits Stage;
	};

	struct ShaderProgramDescription
	{
		/// Shader files
		tString VertexShaderFile;
		tString FragmentShaderFile;

		uint32_t SubpassIndex = 0;
		const RenderTarget* RenderTarget = nullptr;
		VertexInputLayout InputLayout;
		VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		/// Leave empty to run shader reflexion on shader file content.
		/// Use to specify dynamic uniform buffers.
		tDynArray<VkPushConstantRange> PushConstantArray;
		tDynArray<tString> DynamicBuffers;
	};

	class ShaderProgram
	{
	public:
		ShaderProgram();

		static ShaderProgram* Create(const RenderContext& context, const ShaderProgramDescription& description);

		void Destroy(const RenderContext& context);
		bool Reload(const RenderContext& context);
		inline bool IsLoaded() const { return m_pipeline != VK_NULL_HANDLE && m_pipelineLayout != VK_NULL_HANDLE; }

		void UseProgram(VkCommandBuffer cmd);
		void BindDescriptorSets(VkCommandBuffer cmd, const VkDescriptorSet* setArray, uint32_t setCount, uint32_t firstSet = 0, const uint32_t* dynamicOffsetArray = nullptr, uint32_t dynamicOffsetCount = 0);

		inline VkPipelineLayout GetLayout() const { return m_pipelineLayout; }

		const ShaderProgramDescription& GetDescription() const { return m_description; }

	private:
		bool _Create(const RenderContext& context, const ShaderProgramDescription& description);
		ShaderProgramDescription m_description;
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};


	class ShaderFileDB
	{
	public:

		template <size_t _KeySize>
		static void GenerateKey(char(&key)[_KeySize], const char* vertexFile, const char* fragmentFile);

		void Init(const RenderContext& context);
		void Destroy(const RenderContext& context);
		void AddShaderProgram(const RenderContext& context, ShaderProgram* shaderProgram);
		ShaderProgram* FindShaderProgram(const char* vertexFile, const char* fragmentFile) const;
		void ReloadFromFile(const RenderContext& context);

		ShaderProgram** GetShaderArray() { return m_shaderArray.data(); }
		uint32_t GetShaderCount() const { return (uint32_t)m_shaderArray.size(); }

	private:
		tDynArray<ShaderProgram*> m_shaderArray;
		tMap<tString, uint32_t> m_indexMap;
	};

	template <size_t _KeySize>
	void ShaderFileDB::GenerateKey(char(&key)[_KeySize], const char* vertexFile, const char* fragmentFile)
	{
		strcpy_s(key, vertexFile);
		strcat_s(key, fragmentFile);
		char* i = key;
		while (*i)
		{
			*i = tolower(*i);
			++i;
		}
	}
}