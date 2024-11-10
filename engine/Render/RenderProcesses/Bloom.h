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

		tBloomConfig Config;
		ShaderProgram* DownsampleShader;
		ShaderProgram* UpsampleShader;
		ShaderProgram* ComposeShader;
		tArray<RenderTarget, BLOOM_MIPMAP_LEVELS> RenderTargetArray;

		cTexture* InputTarget;
		RenderTarget* ComposeTarget;
	};

}