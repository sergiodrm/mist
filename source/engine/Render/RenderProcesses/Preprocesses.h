#pragma once

#include "Render/Shader.h"
#include "Render/Globals.h"
#include "Render/VulkanBuffer.h"
#include "Render/RenderAPI.h"
#include "Render/RenderTarget.h"
#include "Render/Texture.h"
#include "Render/Model.h"
#include <glm/glm.hpp>
#include "RenderAPI/Device.h"
#include "RenderProcess.h"
#include "Utils/FileSystem.h"

namespace rendersystem
{
	class RenderSystem;
}

namespace Mist
{
	struct PreprocessIrradianceResources
	{
		rendersystem::ShaderProgram* brdfShader;
		rendersystem::ShaderProgram* equirectangularShader;
		rendersystem::ShaderProgram* irradianceShader;
		rendersystem::ShaderProgram* specularShader;
		render::RenderTargetHandle rt;
		cModel cubeModel;

		void Init(rendersystem::RenderSystem* rs);
		void PrepareDraw(rendersystem::RenderSystem* rs, render::Extent2D viewportSize, rendersystem::ShaderProgram* shader);
		void DrawCube(rendersystem::RenderSystem* rs);
		void Destroy(rendersystem::RenderSystem* rs);
	};

	struct PreprocessIrradianceInfo
	{
		uint32_t cubemapWidthHeight;
		uint32_t irradianceCubemapWidthHeight;
		uint32_t specularCubemapWidthHeight;
		cAssetPath hdrFilepath;
		void* userData = nullptr;
	};

	struct PreprocessIrradianceResult
	{
		render::TextureHandle brdf;
		render::TextureHandle cubemap;
		render::TextureHandle irradianceCubemap;
		render::TextureHandle specularCubemap;
	};

	typedef void (*fnPreprocessIrradianceCallback)(const PreprocessIrradianceResult& result, void* userData);

	class Preprocess : public RenderProcess
	{

	public:

		virtual ~Preprocess();
		virtual RenderProcessType GetProcessType() const { return RENDERPROCESS_PREPROCESSES; }

		virtual void Init(const RenderContext& context) override;
		virtual void Destroy(const RenderContext& context) override;
		virtual void InitFrameData(const RenderContext& context, const Renderer& renderFrame, uint32_t frameIndex, UniformBufferMemoryPool& buffer) override {}
		virtual void UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext) override {}
		virtual void Draw(const RenderContext& context, const RenderFrameContext& frameContext) override;

		virtual render::RenderTarget* GetRenderTarget(uint32_t index = 0) const { return nullptr; }

		void PushIrradiancePreprocess(const PreprocessIrradianceInfo& info, fnPreprocessIrradianceCallback fn);
	protected:
		PreprocessIrradianceResult ProcessIrradianceRequest(const PreprocessIrradianceInfo& info);
	private:
		PreprocessIrradianceResources m_irradianceResources;

		static constexpr uint32_t MaxRequests = 8;
		uint32_t m_irradianceRequestCount;
		PreprocessIrradianceInfo m_irradianceInfoArray[MaxRequests];
		fnPreprocessIrradianceCallback m_irradianceFnArray[MaxRequests];
	};
}

