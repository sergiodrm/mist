// header file for Mist project 
#pragma once
#if 0

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <Core/Types.h>
#include <Utils/FileSystem.h>
#include "Render/RenderTypes.h"
#include "Render/VulkanBuffer.h"
#include "Render/RenderAPI.h"

#include "RenderAPI/Device.h"
#include "RenderAPI/ShaderCompiler.h"

namespace Mist
{
	// Forward declarations
	struct RenderContext;
	class DescriptorLayoutCache;
	class RenderPipeline;
	class RenderTarget;
	class cTexture;
	struct SamplerDescription;

	struct ShaderFileDescription
	{
		cAssetPath filepath;
		render::shader_compiler::CompilationOptions options;
	};

	class ShaderCompiler
	{
	public:
		ShaderCompiler(render::Device* device);
		~ShaderCompiler();

		bool Compile(const ShaderFileDescription& desc, render::ShaderType stage);
		render::ShaderHandle GetShader(render::ShaderType stage) const;

		void ClearCachedData();
		void SetUniformBufferAsDynamic(const char* uniformBufferName);

		const render::shader_compiler::ShaderReflectionProperties& GetReflectionProperties() const { return m_properties; }

	private:
		render::Device* m_device;
		tDynArray<render::ShaderHandle> m_shaders;
		render::shader_compiler::ShaderReflectionProperties m_properties;
	};

	struct ShaderDynamicBufferDescription
	{
		bool IsShared = true;
		uint32_t ElemCount = 0;
		tString Name;
	};

	struct tShaderProgramDescription
	{
		/// Shader files
		ShaderFileDescription VertexShaderFile;
		ShaderFileDescription FragmentShaderFile;
		ShaderFileDescription ComputeShaderFile;

		/// Leave empty to run shader reflexion on shader file content.
		/// Use to specify dynamic uniform buffers.
		tDynArray<VkPushConstantRange> PushConstantArray;
		tDynArray<ShaderDynamicBufferDescription> DynamicBuffers;
	};

	struct tShaderParam
	{
		tFixedString<256> Name;
		uint32_t SetIndex;
		uint32_t Binding;
	};

	class tShaderParamAccess
	{
	public:
		void SetupBatch(const RenderContext& context, const ShaderReflectionProperties& reflection, const ShaderDynamicBufferDescription* dynamicBuffers, uint32_t dynamicBuffersCount);
		inline bool HasBatch() const { return m_batchId != UINT32_MAX; }

		void SetBufferData(const RenderContext& context, const char* bufferName, const void* data, uint32_t dataSize);
		void SetDynamicBufferData(const RenderContext& context, const char* bufferName, const void* data, uint32_t elemSize, uint32_t elemCount, uint32_t elemIndexOffset = 0);
		void SetDynamicBufferOffset(const RenderContext& renderContext, const char* bufferName, uint32_t elemSize, uint32_t elemOffset);

		void BindTextureSlot(const RenderContext& context, VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout, VkDescriptorSetLayout setLayout, uint32_t slot, const cTexture& texture, EDescriptorType texType, const Sampler* sampler = nullptr);
		void BindTextureArraySlot(const RenderContext& context, VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout, VkDescriptorSetLayout setLayout, uint32_t slot, const cTexture* const* textures, uint32_t textureCount, EDescriptorType texType, const Sampler* sampler = nullptr);

		inline bool IsDirty() const { return m_dirty; }
		void MarkAsDirty(const RenderContext& context);
		void FlushBatch(const RenderContext& context, VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout);

		const tShaderParam& GetParam(const char* str) const;

		void DumpInfo();

	protected:
		void NewShaderParam(const char* name, bool isShared, uint32_t setIndex, uint32_t binding);

	private:
		uint32_t m_batchId = UINT32_MAX;
		tMap<tString, tShaderParam> m_paramMap;
		bool m_dirty;
	};

	class ShaderProgram
	{
	public:
		ShaderProgram();

		static ShaderProgram* Create(const RenderContext& context, const tShaderProgramDescription& description);

		void Destroy(const RenderContext& context);
		bool Reload(const RenderContext& context);
#if 0
		inline bool IsLoaded() const { return m_pipeline != VK_NULL_HANDLE && m_pipelineLayout != VK_NULL_HANDLE; }
#else
		inline bool IsLoaded() const { return m_vs != nullptr || m_fs != nullptr || m_cs != nullptr; }
#endif

		[[deprecated]]
		void UseProgram(VkCommandBuffer cmd) const;

		void UseProgram(const RenderContext& context);
		void UseProgram(const RenderContext& context, VkCommandBuffer cmd);
		void BindDescriptorSets(VkCommandBuffer cmd, const VkDescriptorSet* setArray, uint32_t setCount, uint32_t firstSet = 0, const uint32_t* dynamicOffsetArray = nullptr, uint32_t dynamicOffsetCount = 0) const;

		void SetSampler(const RenderContext& context,
			EFilterType minFilter = FILTER_LINEAR,
			EFilterType magFilter = FILTER_LINEAR,
			ESamplerAddressMode modeU = SAMPLER_ADDRESS_MODE_REPEAT,
			ESamplerAddressMode modeV = SAMPLER_ADDRESS_MODE_REPEAT,
			ESamplerAddressMode modeW = SAMPLER_ADDRESS_MODE_REPEAT);
		void SetBufferData(const RenderContext& context, const char* bufferName, const void* data, uint32_t size);
		void SetDynamicBufferData(const RenderContext& context, const char* bufferName, const void* data, uint32_t elemSize, uint32_t elemCount, uint32_t elemIndexOffset = 0);
		void SetDynamicBufferOffset(const RenderContext& renderContext, const char* bufferName, uint32_t elemSize, uint32_t elemOffset);

		void BindSampledTexture(const RenderContext& context, const char* uniformName, const cTexture& texture);
		void BindSampledTextureArray(const RenderContext& context, const char* uniformName, const cTexture* const* textureArray, uint32_t textureCount);
		void BindStorageTexture(const RenderContext& context, const char* uniformName, const cTexture& texture);
		void BindStorageTextureArray(const RenderContext& context, const char* uniformName, const cTexture* const* textureArray, uint32_t textureCount);
		void BindTexture(const RenderContext& context, const char* uniformName, const cTexture& texture, EDescriptorType texType);
		void BindTextureArray(const RenderContext& context, const char* uniformName, const cTexture* const* textureArray, uint32_t textureCount, EDescriptorType texType);

		void FlushDescriptors(const RenderContext& context);



		inline const tShaderParam GetParam(const char* paramName) const { return m_paramAccess.GetParam(paramName); }
#if 0

		inline VkPipeline GetPipeline() const { return m_pipeline; }
		inline VkPipelineLayout GetPipelineLayout() const { return m_pipelineLayout; }
		inline VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t index) const { check(index < (uint32_t)m_setLayoutArray.size());  return m_setLayoutArray[index]; }

#endif // 0

		const tShaderProgramDescription& GetDescription() const { return m_description; }

		void DumpInfo();

	private:

		uint32_t FindTextureSlot(const char* textureName, EDescriptorType type) const;

		void SetupDescriptors(const RenderContext& context);
		VkCommandBuffer GetCommandBuffer(const RenderContext& context) const;
		bool _Create(const RenderContext& context, const tShaderProgramDescription& description);
		bool _ReloadGraphics(const RenderContext& context);
		bool _ReloadCompute(const RenderContext& context);
#if 0
		bool GenerateShaderModules(ShaderCompiler& compiler);
		bool GenerateShaderModules(const ShaderFileDescription& fileDesc, render::ShaderType type);
		bool GenerateReflectionProperties(const uint32_t* binarySource, uint32_t binaryCount);
#endif // 0

		tShaderProgramDescription m_description;
		//ShaderReflectionProperties m_reflectionProperties;
		//VkPipeline m_pipeline;
		//VkPipelineLayout m_pipelineLayout;
		//VkPipelineBindPoint m_bindPoint;
		//tDynArray<VkDescriptorSetLayout> m_setLayoutArray;

		render::ShaderHandle m_vs;
		render::ShaderHandle m_fs;
		render::ShaderHandle m_cs;
		render::shader_compiler::ShaderReflectionProperties m_properties;

		tShaderParamAccess m_paramAccess;
		Sampler m_sampler = VK_NULL_HANDLE;
	};

	class ShaderFileDB
	{
	public:
		void Init(const RenderContext& context);
		void Destroy(const RenderContext& context);
		ShaderProgram* AddShaderProgram(const RenderContext& context, const tShaderProgramDescription& description);
		ShaderProgram* FindShaderProgram(const ShaderFileDescription& vertexFileDesc, const ShaderFileDescription& fragFileDesc) const;
		ShaderProgram* FindShaderProgram(const ShaderFileDescription& fileDesc) const;
		ShaderProgram* FindShaderProgram(const char* key) const;
		void ReloadFromFile(const RenderContext& context);

		ShaderProgram** GetShaderArray() { return m_shaderArray.data(); }
		uint32_t GetShaderCount() const { return (uint32_t)m_shaderArray.size(); }

	private:
		tDynArray<ShaderProgram*> m_shaderArray;
		tMap<tString, uint32_t> m_indexMap;
	};
}
#endif // 0
