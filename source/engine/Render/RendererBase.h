// header file for Mist project 
#pragma once
#include "Render/Shader.h"
#include "Render/RenderTypes.h"
#include "Render/RenderTarget.h"
#include "Render/RenderContext.h"
#include "RenderProcesses/RenderProcess.h"
#include "Render/Globals.h"
#include "Core/Types.h"
#include <type_traits>

namespace Mist
{
	class Swapchain;

	class Renderer
	{
	public:
		struct CopyParams
		{
			render::RenderTargetHandle Src = nullptr;
			render::RenderTargetHandle Dst = nullptr;
			render::RenderTargetBlendState blendState;
		};

		void Init(const RenderContext& context, RenderFrameContext* frameContextArray, uint32_t frameContextCount, const Swapchain& swapchain);
		void Destroy(const RenderContext& context);
		void UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext);
		void Draw(const RenderContext& context, const RenderFrameContext& frameContext);
		void DebugRender();
		void ImGuiDraw();

		const RenderProcess* GetRenderProcess(RenderProcessType type) const;
		RenderProcess* GetRenderProcess(RenderProcessType type);

		render::RenderTargetHandle GetLDRTarget() const { return m_ldr; }
		void CopyRenderTarget(const CopyParams& params);

	private:
		class RenderProcess* m_processArray[RENDERPROCESS_COUNT];
		rendersystem::ShaderProgram* m_copyProgram;
		render::RenderTargetHandle m_ldr;
	};
}


