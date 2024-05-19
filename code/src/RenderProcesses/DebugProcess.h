#pragma once

#include "RenderProcess.h"
#include "RenderTarget.h"
#include "VulkanBuffer.h"
#include "Texture.h"
#include <glm/glm.hpp>
#include "Globals.h"
#include "RenderContext.h"

#define SSAO_KERNEL_SAMPLES 64

namespace Mist
{
	class ShaderProgram;

	namespace debugrender
	{
		void DrawLine3D(const glm::vec3& init, const glm::vec3& end, const glm::vec3& color);
		void DrawAxis(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl);
		void DrawAxis(const glm::mat4& transform);
		void DrawSphere(const glm::vec3& pos, float radius, const glm::vec3& color, uint32_t vertices = 16);

		void SetDebugTexture(VkDescriptorSet descriptor);
		void SetDebugClipParams(float nearClip, float farClip);
	}


	class DebugPipeline
	{
		struct FrameData
		{
			VkDescriptorSet CameraSet;
			VkDescriptorSet SetUBO;
		};
	public:
		void Init(const RenderContext& context, const RenderTarget* renderTarget);
		void PushFrameData(const RenderContext& context, UniformBufferMemoryPool* buffer);
		void PrepareFrame(const RenderContext& context, UniformBufferMemoryPool* buffer);
		void Draw(const RenderContext& context, VkCommandBuffer cmd, uint32_t frameIndex);
		void Destroy(const RenderContext& context);
		void ImGuiDraw();
	private:
		// Render State
		ShaderProgram* m_lineShader;
		VertexBuffer m_lineVertexBuffer;

		tDynArray<FrameData> m_frameSet;
		VertexBuffer m_quadVertexBuffer;
		IndexBuffer m_quadIndexBuffer;
		ShaderProgram* m_quadShader;
		Sampler m_depthSampler;
		bool m_debugDepthMap = true;
	};

	class DebugProcess : public RenderProcess
	{
		struct FrameData
		{
			VkDescriptorSet CameraSet;
			VkDescriptorSet SetUBO;
		};
	public:
		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_DEBUG; }
		virtual void Init(const RenderContext& context) override{}
		virtual void InitFrameData(const RenderContext& context, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer) override{}
		virtual void UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext) override{}
		virtual void Draw(const RenderContext& context, const RenderFrameContext& frameContext) override{}
		virtual void Destroy(const RenderContext& context) override{}
		virtual void ImGuiDraw() override{}
		virtual const RenderTarget* GetRenderTarget(uint32_t index /* = 0 */) const override { return nullptr; }
	private:
		// Render State
		ShaderProgram* m_lineShader;
		VertexBuffer m_lineVertexBuffer;

		tDynArray<FrameData> m_frameSet;
		VertexBuffer m_quadVertexBuffer;
		IndexBuffer m_quadIndexBuffer;
		ShaderProgram* m_quadShader;
		Sampler m_depthSampler;
		bool m_debugDepthMap = true;
	};
}