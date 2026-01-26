// header file for Mist project 
#pragma once
#include "RenderProcesses/RenderProcess.h"
#include "Render/Globals.h"
#include "Core/Types.h"

namespace Mist
{
	class Renderer
	{
	public:
		void Init(rendersystem::RenderSystem* rs, IRenderEngine* engine);
		void Destroy(rendersystem::RenderSystem* rs);
		void Update();
		void Draw(rendersystem::RenderSystem* rs);
		void DebugRender();
		void ImGuiDraw();

		const RenderProcess* GetRenderProcess(RenderProcessType type) const;
		RenderProcess* GetRenderProcess(RenderProcessType type);
		template<typename T>
		T* GetRenderProcessAs();
		template<typename T>
		const T* GetRenderProcessAs() const;

		uint32_t GetRenderProcessCount() const { return RENDERPROCESS_COUNT; }

		render::RenderTargetHandle GetLDRTarget() const { return m_ldr; }
	private:
		class RenderProcess* m_processArray[RENDERPROCESS_COUNT];
		render::RenderTargetHandle m_ldr;
	};

	template <typename T>
	inline T* Renderer::GetRenderProcessAs()
	{
		return static_cast<T*>(GetRenderProcess(T::GetProcessTypeStatic()));
	}

	template <typename T>
	inline const T* Renderer::GetRenderProcessAs() const
	{
		return static_cast<T*>(GetRenderProcess(T::GetProcessTypeStatic()));
	}
}


