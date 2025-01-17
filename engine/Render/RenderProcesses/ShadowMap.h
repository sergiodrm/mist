#pragma once

#include "RenderProcess.h"
#include "Render/RenderTarget.h"
#include "Render/Globals.h"
#include "Render/VulkanBuffer.h"
#include "Render/Texture.h"
#include "Utils/Angles.h"


namespace Mist
{
	struct RenderContext;
	class ShaderProgram;
	class Scene;



	class ShadowMapPipeline
	{
	public:
		enum EShadowMapProjectionType
		{
			PROJECTION_PERSPECTIVE,
			PROJECTION_ORTHOGRAPHIC,
		};

		ShadowMapPipeline();
		~ShadowMapPipeline();

		void Init(const RenderContext& renderContext, const RenderTarget* renderTarget);
		void Destroy(const RenderContext& renderContext);

		void AddFrameData(const RenderContext& renderContext, UniformBufferMemoryPool* buffer);

		void SetPerspectiveClip(float nearClip, float farClip);
		void SetOrthographicClip(float nearClip, float farClip);
		glm::mat4 GetProjection(EShadowMapProjectionType projType) const;
		void SetProjection(float fov, float aspectRatio);
		void SetProjection(float minX, float maxX, float minY, float maxY);
		void SetupLight(uint32_t lightIndex, const glm::vec3& lightPos, const tAngles& lightRot, const glm::mat4& lightProj, const glm::mat4& viewMatrix);

		void SetupSpotLight(uint32_t lightIndex, const glm::mat4& cameraView, const glm::vec3& pos, const tAngles& rot, float cutoff, float nearClip = 0.1f, float farClip = 1000.f);
		void SetupDirectionalLight(uint32_t lightIndex, const glm::mat4& cameraView, const glm::mat4& cameraProj, const tAngles& lightRot, float nearClip = 0.1f, float farClip = 1000.f);

		void FlushToUniformBuffer(const RenderContext& renderContext, UniformBufferMemoryPool* buffer);
		void RenderShadowMap(const RenderContext& context, const Scene* scene, uint32_t lightIndex);
		const glm::mat4& GetDepthVP(uint32_t index) const;
		const glm::mat4& GetLightVP(uint32_t index) const;
		void SetDepthVP(uint32_t index, const glm::mat4& mat);
		void SetLightVP(uint32_t index, const glm::mat4& mat);
		uint32_t GetBufferSize() const;

		void ImGuiDraw(bool createWindow = false);

	protected:
		glm::mat4 ComputeShadowVolume(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& lightDir, float nearClip, float farClip, float splitLambda = 0.95f);
		void SetupLight(uint32_t lightIndex, const glm::mat4& depthViewProjection, const glm::mat4& view);
	private:
		// Shader shadowmap pipeline
		ShaderProgram* m_shader;
		// Cache for save depth view projection data until flush to gpu buffer.
		glm::mat4 m_depthMVPCache[globals::MaxShadowMapAttachments];
		glm::mat4 m_lightMVPCache[globals::MaxShadowMapAttachments];
	public:
		// Projection params
		float m_perspectiveParams[4];
		float m_orthoParams[6];
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
		ShadowMapProcess();
		virtual RenderProcessType GetProcessType() const override { return RENDERPROCESS_SHADOWMAP; }
		virtual void Init(const RenderContext& context) override;
		virtual void Destroy(const RenderContext& renderContext) override;
		virtual void InitFrameData(const RenderContext& renderContext, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer) override;
		virtual void UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& renderFrameContext) override;
		virtual void Draw(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext) override;
		virtual void ImGuiDraw() override;
		virtual const RenderTarget* GetRenderTarget(uint32_t index) const override;

		const ShadowMapPipeline& GetPipeline() const { return m_shadowMapPipeline; }

		ShadowMapPipeline m_shadowMapPipeline;
	private:
		virtual void DebugDraw(const RenderContext& context) override;
	private:
		tArray<RenderTarget, globals::MaxShadowMapAttachments> m_shadowMapTargetArray;
		uint32_t m_lightCount = 0;
		EDebugMode m_debugMode = DEBUG_NONE;
		uint32_t m_debugIndex = 0;
		struct
		{
			bool show = false;
			glm::vec3 pos;
			tAngles rot;
			float cutoff;
			float clips[2];
		} m_debugLightParams;
		struct
		{
			bool show = false;
			glm::vec3 pos;
			tAngles rot;
			float clips[6];
		} m_debugDirParams;
	};
}