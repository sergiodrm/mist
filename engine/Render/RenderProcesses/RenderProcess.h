#pragma once

#include <cstdint>

namespace Mist
{
	struct RenderContext;
	struct RenderFrameContext;
	class UniformBufferMemoryPool;
	class RenderTarget;

#define RENDER_PROCESS_LIST \
	_X_(RENDERPROCESS_GBUFFER) \
	_X_(RENDERPROCESS_SHADOWMAP) \
	_X_(RENDERPROCESS_SSAO) \
	_X_(RENDERPROCESS_LIGHTING) \
	_X_(RENDERPROCESS_DEBUG) \
	_X_(RENDERPROCESS_UI) \
	_X_(RENDERPROCESS_COUNT) \


	enum RenderProcessType
	{
#define _X_(x) x,
		RENDER_PROCESS_LIST
#undef _X_
	};

#define RenderFlagList \
	_X_(None, 0) \
	_X_(Fixed, 1) \
	_X_(ShadowMap, 2) \
	_X_(NoTextures, 4) \
	_X_(Emissive, 8) \

	enum  eRenderFlags
	{
#define _X_(x, v) RenderFlags_##x = v,
		RenderFlagList
#undef _X_
	};

	class Renderer;

	class RenderProcess
	{
	public:
		virtual RenderProcessType GetProcessType() const = 0;
		virtual void Init(const RenderContext& context) = 0;
		virtual void Destroy(const RenderContext& context) = 0;
		virtual void InitFrameData(const RenderContext& context, const Renderer& renderFrame, uint32_t frameIndex, UniformBufferMemoryPool& buffer) = 0;

		virtual void UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext) = 0;
		virtual void Draw(const RenderContext& context, const RenderFrameContext& frameContext) = 0;
		virtual void ImGuiDraw() {}
		virtual void DebugDraw(const RenderContext& context) {}

		virtual const RenderTarget* GetRenderTarget(uint32_t index = 0) const = 0;
	};
}