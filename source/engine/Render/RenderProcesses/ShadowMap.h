#pragma once

#include "RenderProcess.h"
#include "Render/Globals.h"
#include "Utils/Angles.h"
#include "../Camera.h"
#include "../Culling.h"

namespace rendersystem
{
	class RenderSystem;
}

namespace Mist
{
	class Scene;
	class Frustum;
	class Camera;

	glm::mat4 GetSpotLightProjection(float cutOff, float nearClip, float farClip);

	struct ShadowMapTransform
	{
		glm::mat4 proj;
		glm::mat4 view;

		inline glm::mat4 GetVP() const { return (proj * view); }
		inline glm::mat4 GetVPBiased() const
		{
			static constexpr glm::mat4 bias =
			{
				0.5f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.5f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.5f, 0.5f, 0.0f, 1.0f
			};
			return bias * GetVP();
		}
	};

	class ShadowMapProcess : public RenderProcess
	{
		enum EDebugMode
		{
			DEBUG_NONE,
			DEBUG_SINGLE_RT,
			DEBUG_ALL
		};
	public:
		ShadowMapProcess(Renderer* renderer, IRenderEngine* engine);

		static RenderProcessType GetProcessTypeStatic() { return RENDERPROCESS_SHADOWMAP; }
		virtual RenderProcessType GetProcessType() const override { return GetProcessTypeStatic(); }

		virtual void Init(rendersystem::RenderSystem* rs) override;
		virtual void Destroy(rendersystem::RenderSystem* rs) override;
		virtual void Update();
		virtual void Draw(rendersystem::RenderSystem* rs) override;
		virtual void ImGuiDraw() override;
		virtual render::RenderTarget* GetRenderTarget(uint32_t index) const override;

		inline const ShadowMapTransform& GetShadowMapTransform(uint32_t index) const { check(index < globals::MaxOverlappedFrames); return m_shadowTransfoms[index]; }
		inline const render::TextureHandle& GetShadowMap(uint32_t index) const { check(index < globals::MaxOverlappedFrames); return m_shadowMapTargetArray[index]->m_description.depthStencilAttachment.texture; }

		// Compute projection to fit camera reference frustum
		uint32_t SetupDirectionalLight(const Camera& cameraReference, const glm::mat4& lightView, float nearClip = 1.f, float farClip = 100.f, float zmult = 10.f);
		// Fixed projection.
		uint32_t SetupDirectionalLight(const glm::mat4& lightView, float left, float right, float top, float bottom, float nearClip, float farClip);
		uint32_t SetupSpotLight(const glm::mat4& lightView, float cutoff, float nearClip, float farClip);

		uint32_t SetupShadowMap(const glm::mat4& view, const glm::mat4& proj, const glm::vec3* debugColor = nullptr);

	private:
		virtual void DebugDraw() override;
		void InitShadowMaps(rendersystem::RenderSystem* rs, const render::Extent2D& resolution);

		render::RenderTargetHandle CreateDepthRenderTarget(rendersystem::RenderSystem* rs, const render::Extent2D& extent);
	private:
		render::Extent2D m_shadowMapResolution;
		rendersystem::ShaderProgram* m_shader;
		tStaticArray<render::RenderTargetHandle, globals::MaxShadowMapAttachments> m_shadowMapTargetArray;

		ShadowMapTransform m_shadowTransfoms[globals::MaxShadowMapAttachments];
		uint32_t m_renderListIds[globals::MaxShadowMapAttachments];
		uint32_t m_lightCount = 0;

		EDebugMode m_debugMode = DEBUG_NONE;
		uint32_t m_textureDebugIndex = 0;
		CameraController m_camera;

		Frustum m_cameraFrustum;
		Frustum m_lightFrustum;
		bool m_test;
	};
}