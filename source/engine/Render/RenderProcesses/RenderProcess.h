#pragma once

#include <cstdint>
#include "RenderAPI/Device.h"

namespace rendersystem
{
    class ShaderProgram;
	class RenderSystem;
}

namespace Mist
{

#define RENDER_PROCESS_LIST \
	_X_(RENDERPROCESS_PREPROCESSES) \
	_X_(RENDERPROCESS_GBUFFER) \
	_X_(RENDERPROCESS_SHADOWMAP) \
	_X_(RENDERPROCESS_SSAO) \
	_X_(RENDERPROCESS_FORWARD_LIGHTING) \
	_X_(RENDERPROCESS_LIGHTING) \
	_X_(RENDERPROCESS_COUNT) \


	enum RenderProcessType
	{
#define _X_(x) x,
		RENDER_PROCESS_LIST
#undef _X_
	};

	static const char* const RenderProcessNames[] =
	{
#define _X_(x) #x,
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
	class IRenderEngine;

	class RenderProcess
	{
	public:
		RenderProcess(Renderer* renderer, IRenderEngine* engine)
			: m_renderer(renderer), m_engine(engine)
		{
			check(m_renderer);
			check(m_engine);
		}

		virtual ~RenderProcess() {}
		virtual RenderProcessType GetProcessType() const = 0;
		virtual void Init(rendersystem::RenderSystem* rs) = 0;
		virtual void Destroy(rendersystem::RenderSystem* rs) = 0;

		virtual void Draw(rendersystem::RenderSystem* rs) = 0;
		virtual void ImGuiDraw() {}
		virtual void DebugDraw() {}

		virtual render::RenderTarget* GetRenderTarget(uint32_t index = 0) const = 0;
		Renderer* GetRenderer() const { return m_renderer; }
		IRenderEngine* GetEngine() const { return m_engine; }
	private:
		Renderer* m_renderer;
		IRenderEngine* m_engine;
	};
}