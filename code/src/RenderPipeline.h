#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "VulkanBuffer.h"

namespace vkmmc
{
	// Forward declarations
	struct RenderContext;
	class RenderPipeline;
	class RenderTarget;
	
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
		VkDescriptorSetLayout* SetLayoutArray = nullptr;
		uint32_t SetLayoutCount = 0;

		VkPushConstantRange* PushConstantArray = nullptr;
		uint32_t PushConstantCount = 0;

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

		void AddShaderProgram(const RenderContext& context, ShaderProgram* shaderProgram);
		ShaderProgram* FindShaderProgram(const char* vertexFile, const char* fragmentFile) const;
		void Destroy(const RenderContext& context);

	private:
		tDynArray<ShaderProgram*> m_pipelineArray;
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