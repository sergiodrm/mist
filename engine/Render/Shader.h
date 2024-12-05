// header file for Mist project 
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <Core/Types.h>
#include <Utils/FileSystem.h>
#include "Render/RenderTypes.h"
#include "Render/VulkanBuffer.h"
#include "Render/RenderAPI.h"

namespace Mist
{
	// Forward declarations
	struct RenderContext;
	class DescriptorLayoutCache;
	class RenderPipeline;
	class RenderTarget;
	class cTexture;
	struct SamplerDescription;


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
		tDynArray<ShaderBindingDescriptorInfo> BindingArray;
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
		tDynArray<ShaderDescriptorSetInfo> DescriptorSetInfoArray;
		std::unordered_map<VkShaderStageFlags, ShaderPushConstantBufferInfo> PushConstantMap;
	};


	struct tCompileMacroDefinition
	{
		tFixedString<64> Macro;
		tFixedString<64> Value;
		tCompileMacroDefinition()
		{
			Macro[0] = 0;
			Value[0] = 0;
		}
		tCompileMacroDefinition(const char* str)
		{
			Macro.Set(str);
			Value[0] = 0;
		}
	};

	struct tCompileOptions
	{
		bool GenerateDebugInfo;
		char EntryPointName[64];
		Mist::tDynArray<tCompileMacroDefinition> MacroDefinitionArray;

		tCompileOptions()
			: GenerateDebugInfo(true), EntryPointName{ "main" }
		{		}
	};

	struct ShaderFileDescription
	{
		cAssetPath Filepath;
		tCompileOptions CompileOptions;
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
		bool ProcessShaderFile(const char* filepath, VkShaderStageFlagBits shaderStage, const tCompileOptions& compileOptions);
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
		static bool GetSpvBinaryFromFile(const char* shaderFilepath, const char* binaryFilepath, uint32_t** binaryData, size_t* binaryCount);
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

	enum class tShaderType
	{
		Graphics,
		Compute
	};

	struct tShaderDynamicBufferDescription
	{
		bool IsShared = true;
		uint32_t ElemCount = 0;
		tString Name;
	};

	struct tShaderProgramDescription
	{
		tShaderType Type = tShaderType::Graphics;
		/// Shader files
		ShaderFileDescription VertexShaderFile;
		ShaderFileDescription FragmentShaderFile;
		ShaderFileDescription ComputeShaderFile;

		// Pipeline config
		uint32_t SubpassIndex = 0;
		const RenderTarget* RenderTarget = nullptr;
		VertexInputLayout InputLayout;
		EPrimitiveTopology Topology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		ECullMode CullMode = CULL_MODE_FRONT_BIT;
		EFrontFace FrontFaceMode = FRONT_FACE_CLOCKWISE;
		EDepthStencilState DepthStencilMode = DEPTH_STENCIL_DEPTH_TEST | DEPTH_STENCIL_DEPTH_WRITE;
		tDynArray<tColorBlendState> ColorAttachmentBlendingArray;
		tDynArray<EDynamicState> DynamicStates;

		/// Leave empty to run shader reflexion on shader file content.
		/// Use to specify dynamic uniform buffers.
		tDynArray<VkPushConstantRange> PushConstantArray;
		tDynArray<tShaderDynamicBufferDescription> DynamicBuffers;
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
		void SetupBatch(const RenderContext& context, const ShaderReflectionProperties& reflection, const tShaderDynamicBufferDescription* dynamicBuffers, uint32_t dynamicBuffersCount);
		inline bool HasBatch() const { return m_batchId != UINT32_MAX; }

		void SetBufferData(const RenderContext& context, const char* bufferName, const void* data, uint32_t dataSize);
		void SetDynamicBufferData(const RenderContext& context, const char* bufferName, const void* data, uint32_t elemSize, uint32_t elemCount, uint32_t elemIndexOffset = 0);
		void SetDynamicBufferOffset(const RenderContext& renderContext, const char* bufferName, uint32_t elemSize, uint32_t elemOffset);

		void BindTextureSlot(const RenderContext& context, VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout, uint32_t slot, const cTexture& texture, const Sampler* sampler = nullptr);
		void BindTextureArraySlot(const RenderContext& context, VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout, uint32_t slot, const cTexture* const* textures, uint32_t textureCount, const Sampler* sampler = nullptr);

		void MarkAsDirty(const RenderContext& context);
		void FlushBatch(const RenderContext& context, VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout);

		const tShaderParam& GetParam(const char* str) const;

		void DumpInfo();

	protected:
		void NewShaderParam(const char* name, bool isShared, uint32_t setIndex, uint32_t binding);

	private:
		uint32_t m_batchId = UINT32_MAX;
		tMap<tString, tShaderParam> m_paramMap;
	};

	class ShaderProgram
	{
	public:
		ShaderProgram();

		static ShaderProgram* Create(const RenderContext& context, const tShaderProgramDescription& description);

		void Destroy(const RenderContext& context);
		bool Reload(const RenderContext& context);
		inline bool IsLoaded() const { return m_pipeline != VK_NULL_HANDLE && m_pipelineLayout != VK_NULL_HANDLE; }

		[[deprecated]]
		void UseProgram(CommandBuffer cmd) const;

		void UseProgram(const RenderContext& context);
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
		void BindTextureSlot(const RenderContext& context, uint32_t slot, const cTexture& texture);
		void BindTextureArraySlot(const RenderContext& context, uint32_t slot, const cTexture* const* textureArray, uint32_t textureCount);
		void FlushDescriptors(const RenderContext& context);

		inline const tShaderParam GetParam(const char* paramName) const { return m_paramAccess.GetParam(paramName); }

		inline VkPipelineLayout GetLayout() const { return m_pipelineLayout; }

		const tShaderProgramDescription& GetDescription() const { return m_description; }

		void DumpInfo();

	private:
		void SetupDescriptors(const RenderContext& context);
		VkCommandBuffer GetCommandBuffer(const RenderContext& context) const;
		bool _Create(const RenderContext& context, const tShaderProgramDescription& description);
		bool _ReloadGraphics(const RenderContext& context);
		bool _ReloadCompute(const RenderContext& context);
		tShaderProgramDescription m_description;
		ShaderReflectionProperties m_reflectionProperties;
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
		VkPipelineBindPoint m_bindPoint;
		tDynArray<VkDescriptorSetLayout> m_setLayoutArray;

		tShaderParamAccess m_paramAccess;
		Sampler m_sampler = VK_NULL_HANDLE;
	};

	class ShaderFileDB
	{
	public:
		void Init(const RenderContext& context);
		void Destroy(const RenderContext& context);
		void AddShaderProgram(const RenderContext& context, ShaderProgram* shaderProgram);
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