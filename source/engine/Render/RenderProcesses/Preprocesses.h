#pragma once

#include "Render/Globals.h"
#include "Render/Model.h"
#include <glm/glm.hpp>
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
		void PrepareDraw(rendersystem::RenderSystem* rs, const render::RenderTargetHandle& _rt, render::Extent2D viewportSize, rendersystem::ShaderProgram* shader);
		void DrawCube(rendersystem::RenderSystem* rs);
		void Destroy(rendersystem::RenderSystem* rs);
	};

	struct PreprocessIrradianceInfo
	{
		uint32_t cubemapWidthHeight;
		uint32_t irradianceCubemapWidthHeight;
		uint32_t specularCubemapWidthHeight;
		glm::vec3 minCubemapClamp;
		glm::vec3 maxCubemapClamp;
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

		Preprocess(Renderer* renderer, IRenderEngine* engine);
		virtual ~Preprocess();
		virtual RenderProcessType GetProcessType() const { return RENDERPROCESS_PREPROCESSES; }

		virtual void Init(rendersystem::RenderSystem* rs) override;
		virtual void Destroy(rendersystem::RenderSystem* rs) override;
		virtual void Draw(rendersystem::RenderSystem* rs) override;

		virtual render::RenderTarget* GetRenderTarget(uint32_t index = 0) const { return nullptr; }

		void PushIrradiancePreprocess(const PreprocessIrradianceInfo& info, fnPreprocessIrradianceCallback fn);
	protected:
		PreprocessIrradianceResult ProcessIrradianceRequest(rendersystem::RenderSystem* rs, const PreprocessIrradianceInfo& info);
	private:
		PreprocessIrradianceResources m_irradianceResources;

		static constexpr uint32_t MaxRequests = 8;
		uint32_t m_irradianceRequestCount;
		PreprocessIrradianceInfo m_irradianceInfoArray[MaxRequests];
		fnPreprocessIrradianceCallback m_irradianceFnArray[MaxRequests];
	};
}

