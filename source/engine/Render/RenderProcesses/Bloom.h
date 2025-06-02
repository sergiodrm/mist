#pragma once

#include "RenderProcess.h"
#include "Render/RenderTarget.h"
#include "Render/Globals.h"
#include "Render/VulkanBuffer.h"
#include "Render/Texture.h"
#include <glm/glm.hpp>
#include "RenderAPI/Device.h"

#define BLOOM_MIPMAP_LEVELS 5

namespace rendersystem
{
	class ShaderProgram;
}

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
			BLOOM_DEBUG_FILTER_PASS,
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
		render::RenderTargetHandle m_composeTarget;
		// Texture to be blurred by downsampling and upsampling.
		render::TextureHandle m_inputTarget;
		// Texture to be mixed with the result of bloom process.
		render::TextureHandle m_blendTexture = nullptr;

	private:
		rendersystem::ShaderProgram* m_filterShader;
		rendersystem::ShaderProgram* m_downsampleShader;
		rendersystem::ShaderProgram* m_upsampleShader;
		rendersystem::ShaderProgram* m_composeShader;
		tArray<render::RenderTargetHandle, BLOOM_MIPMAP_LEVELS> m_renderTargetArray;
		tArray<render::TextureHandle, BLOOM_MIPMAP_LEVELS> m_renderTargetTexturesArray;

		float m_threshold = 1.f;
		float m_knee = 0.1f;
	};
}