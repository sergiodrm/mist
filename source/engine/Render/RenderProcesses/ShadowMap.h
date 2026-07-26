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

	glm::mat4 GetSpotLightProjection(float cutOff, float nearClip, float farClip);

	struct ShadowMap
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

		inline const ShadowMap& GetShadowMapData(uint32_t index) const { check(index < globals::MaxOverlappedFrames); return m_shadowMaps[index]; }

		uint32_t SetupDirectionalLight(const glm::vec3& pos, const tAngles& rot, float left, float right, float top, float bottom, float nearClip, float farClip);
		uint32_t SetupSpotLight(const glm::vec3& pos, const tAngles& rot, float cutoff, float nearClip, float farClip);

	private:
		virtual void DebugDraw() override;
	private:
		ShadowMap m_shadowMaps[globals::MaxShadowMapAttachments];
		uint32_t m_renderListIds[globals::MaxShadowMapAttachments];
		uint32_t m_lightCount = 0;
		float m_zmult = 100.f;
		float m_directionalClipFar = 100.f;

		rendersystem::ShaderProgram* m_shader;
		tArray<render::RenderTargetHandle, globals::MaxShadowMapAttachments> m_shadowMapTargetArray;


		EDebugMode m_debugMode = DEBUG_NONE;
		uint32_t m_textureDebugIndex = 0;
		CameraController m_camera;

		Frustum m_cameraFrustum;
		Frustum m_lightFrustum;
		bool m_test;
	};
}