#pragma once

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
	class UniformBuffer;
	class GBuffer;

	class SSAO
	{
		struct SSAOUBO
		{
			glm::vec2 NoiseScale;
			float Radius;
			glm::vec3 KernelSamples[SSAO_KERNEL_SAMPLES];
			glm::mat4 Projection;
		};

		struct FrameData
		{
			VkDescriptorSet SSAOSet;
		};
	public:
	
		void Init(const RenderContext& renderContext);
		void Destroy(const RenderContext& renderContext);
		void InitFrameData(const RenderContext& context, UniformBuffer* buffer, uint32_t frameIndex, const GBuffer& gbuffer);
		void PrepareFrame(const RenderContext& renderContext, RenderFrameContext& frameContext);
		void DrawPass(const RenderContext& renderContext, const RenderFrameContext& frameContext);
		const RenderTarget& GetRenderTarget() const { return m_rt; }

	private:
		ShaderProgram* m_ssaoShader;
		RenderTarget m_rt;
		glm::vec3 m_ssaoKernel[SSAO_KERNEL_SAMPLES];
		Texture m_noiseTexture;
		Sampler m_noiseSampler;
		FrameData m_frameData[globals::MaxOverlappedFrames];
		SSAOUBO m_uboData;
		VertexBuffer m_screenQuadVB;
		IndexBuffer m_screenQuadIB;
	};

}