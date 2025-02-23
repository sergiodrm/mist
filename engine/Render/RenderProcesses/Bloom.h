#pragma once

#include "RenderProcess.h"
#include "Render/RenderTarget.h"
#include "Render/Globals.h"
#include "Render/VulkanBuffer.h"
#include "Render/Texture.h"
#include <glm/glm.hpp>

#define BLOOM_MIPMAP_LEVELS 5

namespace Mist
{
	struct RenderContext;
	class ShaderProgram;

	struct tBloomConfig
	{
		enum
		{
			BLOOM_DISABLED,
			BLOOM_ACTIVE,
			BLOOM_DEBUG_EMISSIVE_PASS,
			BLOOM_DEBUG_DOWNSCALE_PASS
		};
		uint32_t BloomMode = BLOOM_ACTIVE;
		float MixCompositeAlpha = 0.5f;
		float UpscaleFilterRadius = 0.005f;
	};

	class BloomEffect
	{
	public:
		BloomEffect();

		void Init(const RenderContext& context);
		void Draw(const RenderContext& context);
		void Destroy(const RenderContext& context);


		void ImGuiDraw();


		// Config of bloom draw. Can be updated before each draw.
		tBloomConfig m_config;
		// Output mixing.
		RenderTarget* m_composeTarget;
		// Texture to be blurred by downsampling and upsampling.
		const cTexture* m_inputTarget;
		// Texture to be mixed with the result of bloom process.
		const cTexture* m_blendTexture = nullptr;

	private:
		ShaderProgram* m_filterShader;
		ShaderProgram* m_downsampleShader;
		ShaderProgram* m_upsampleShader;
		ShaderProgram* m_composeShader;
		tArray<RenderTarget, BLOOM_MIPMAP_LEVELS> m_renderTargetArray;

		float m_threshold = 1.f;
		float m_knee = 0.1f;
	};
}