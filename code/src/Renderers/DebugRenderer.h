// Autogenerated code for Mist project
// Header file

#pragma once
#include "RendererBase.h"
#include "Texture.h"
#include <glm/glm.hpp>

namespace Mist
{
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
		void PushFrameData(const RenderContext& context, UniformBuffer* buffer);
		void PrepareFrame(const RenderContext& context, UniformBuffer* buffer);
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
#if 0

	class DebugRenderer : public IRendererBase
	{

		struct QuadVertex
		{
			glm::vec3 Position;
			glm::vec2 UVs;
		};
	public:
		virtual void Init(const RendererCreateInfo& info) override;
		virtual void Destroy(const RenderContext& renderContext) override;
		virtual void PrepareFrame(const RenderContext& renderContext, RenderFrameContext& renderFrameContext) override;
		virtual void RecordCmd(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext, uint32_t attachmentIndex) override;
		virtual void ImGuiDraw() override;
		virtual VkImageView GetRenderTarget(uint32_t currentFrameIndex, uint32_t attachmentIndex) const override;
	protected:
		// Render State
		RenderPipeline m_renderPipeline;
		VertexBuffer m_lineVertexBuffer;

		std::vector<FrameData> m_frameData;
		VertexBuffer m_quadVertexBuffer;
		IndexBuffer m_quadIndexBuffer;
		RenderPipeline m_quadPipeline;
		VkSampler m_depthSampler;
		bool m_debugDepthMap = true;
	};

#endif // 0

}
