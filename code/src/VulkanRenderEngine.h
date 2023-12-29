
#pragma once

#include "RenderEngine.h"
#include "RenderPass.h"
#include "RenderHandle.h"
#include "RenderPipeline.h"
#include "RenderObject.h"
#include "RenderTexture.h"
#include "VulkanBuffer.h"
#include "Debug.h"
#include "Swapchain.h"
#include "Mesh.h"
#include "FunctionStack.h"
#include "Framebuffer.h"
#include <cstdio>

#include <vk_mem_alloc.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <string.h>

namespace vkmmc
{
	class Framebuffer;
	struct ShaderModuleLoadDescription;

	struct Window
	{
		SDL_Window* WindowInstance;
		uint32_t Width;
		uint32_t Height;
		char Title[32];

		Window() : WindowInstance(nullptr), Width(1920), Height(1080)
		{
			*Title = 0;
		}

		static Window Create(uint32_t width, uint32_t height, const char* title);
	};

	struct UploadContext
	{
		VkFence Fence;
		VkCommandPool CommandPool;
		VkCommandBuffer CommandBuffer;
	};

	struct GPUCamera
	{
		glm::mat4 View;
		glm::mat4 Projection;
		glm::mat4 ViewProjection;
	};

	struct GPUObject
	{
		glm::mat4 ModelTransform;
	};

	struct MaterialTextureData
	{
		enum ESamplerIndex
		{
			SAMPLER_INDEX_DIFFUSE,
			SAMPLER_INDEX_NORMAL,
			SAMPLER_INDEX_SPECULAR,

			SAMPLER_INDEX_COUNT
		};

		VkDescriptorSet Set;
		VkSampler ImageSamplers[SAMPLER_INDEX_COUNT];

		MaterialTextureData() : Set(VK_NULL_HANDLE) { for (uint32_t i = 0; i < SAMPLER_INDEX_COUNT; ++i) ImageSamplers[i] = VK_NULL_HANDLE; }
	};

	struct MeshRenderData
	{
		VertexBuffer VertexBuffer;
		IndexBuffer IndexBuffer;
	};

	struct RenderObjectContainer
	{
		static constexpr size_t MaxRenderObjects = 50000;
		std::array<RenderObjectTransform, MaxRenderObjects> Transforms;
		std::array<RenderObjectMesh, MaxRenderObjects> Meshes;
		uint32_t Counter = 0;

		uint32_t New() { check(Counter < MaxRenderObjects); return Counter++; }
		uint32_t Count() const { return Counter; }
	};

	class VulkanRenderEngine : public IRenderEngine
	{
		static constexpr size_t MaxOverlappedFrames = 2;
	public:
		virtual bool Init(const InitializationSpecs& initSpec) override;
		virtual void RenderLoop() override;
		virtual bool RenderProcess() override;
		virtual void Shutdown() override;

		virtual void UpdateSceneView(const glm::mat4& view, const glm::mat4& projection) override;

		virtual void UploadMesh(Mesh& mesh) override;
		virtual void UploadMaterial(Material& material) override;
		virtual RenderHandle LoadTexture(const char* filename) override;

		virtual RenderObject NewRenderObject() override;
		virtual RenderObjectTransform* GetObjectTransform(RenderObject object) override;
		virtual RenderObjectMesh* GetObjectMesh(RenderObject object) override;
		virtual uint32_t GetObjectCount() const { return m_scene.Count(); }
		virtual void AddImGuiCallback(std::function<void()>&& fn) { m_imguiCallbackArray.push_back(fn); }

	protected:
		void Draw();
		void DrawRenderables(VkCommandBuffer cmd, const RenderObjectTransform* transforms, const RenderObjectMesh* meshes, size_t count);
		void WaitFence(VkFence fence, uint64_t timeoutSeconds = 1e9);
		RenderFrameContext& GetFrameContext();
		void ImmediateSubmit(std::function<void(VkCommandBuffer)>&& fn);
		void ImGuiNewFrame();
		void ImGuiProcessEvent(const SDL_Event& e);

		// Initializations
		bool InitVulkan();
		bool InitCommands();
		bool InitFramebuffers();
		bool InitSync();
		bool InitPipeline();
		bool InitImGui();

		// Builder utils
		RenderPipeline CreatePipeline(
			const ShaderModuleLoadDescription* shaderStagesDescriptions,
			size_t shaderStagesCount,
			const VkPipelineLayoutCreateInfo& layoutInfo,
			const VertexInputLayout& inputDescription
		);
		RenderHandle RegisterPipeline(RenderPipeline pipeline);

		bool InitMaterial(const Material& materialHandle, MaterialTextureData& material);
		void SubmitMaterialTexture(MaterialTextureData& material, MaterialTextureData::ESamplerIndex sampler, const VkImageView& imageView);

		void UpdateBufferData(GPUBuffer* buffer, const void* data, uint32_t size);

	private:

		Window m_window;
		
		RenderContext m_renderContext;
		
		Swapchain m_swapchain;
		RenderPass m_renderPass;
		RenderHandle m_handleRenderPipeline;
		RenderHandle m_defaultTexture;
		std::vector<VkFramebuffer> m_framebuffers;
		std::vector<Framebuffer> m_framebufferArray;

		RenderFrameContext m_frameContextArray[MaxOverlappedFrames];
		size_t m_frameCounter;

		DescriptorAllocator m_descriptorAllocator;
		DescriptorLayoutCache m_descriptorLayoutCache;

		enum EDescriptorLayoutType
		{
			DESCRIPTOR_SET_GLOBAL_LAYOUT,
			DESCRIPTOR_SET_TEXTURE_LAYOUT,

			DESCRIPTOR_SET_COUNT
		};
		VkDescriptorSetLayout m_descriptorLayouts[DESCRIPTOR_SET_COUNT];
		VkDescriptorSet m_descriptorSets[DESCRIPTOR_SET_COUNT];

		template <typename RenderResourceType>
		using ResourceMap = std::unordered_map<RenderHandle, RenderResourceType, RenderHandle::Hasher>;
		ResourceMap<RenderPipeline> m_pipelines;
		ResourceMap<RenderTexture> m_textures;
		ResourceMap<MeshRenderData> m_meshRenderData;
		ResourceMap<MaterialTextureData> m_materials;

		RenderObjectContainer m_scene;
		bool m_dirtyCachedCamera;
		GPUCamera m_cachedCameraData;

		UploadContext m_immediateSubmitContext;

		FunctionStack m_shutdownStack;
		typedef std::function<void()> ImGuiCallback;
		std::vector<ImGuiCallback> m_imguiCallbackArray;
	};
}