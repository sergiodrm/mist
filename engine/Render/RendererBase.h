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
	struct tCopyShaderKey
	{
		RenderTargetDescription RTDesc;
		tColorBlendState BlendState;

		inline bool operator ==(const tCopyShaderKey& other) const { return RTDesc == other.RTDesc && BlendState == other.BlendState; }
		inline bool operator !=(const tCopyShaderKey& other) const { return !(*this).operator ==(other); }
	};
}

template<>
struct std::hash<Mist::tCopyShaderKey>
{
	inline std::size_t operator()(const Mist::tCopyShaderKey& v) const
	{
		std::size_t h = 0;
		Mist::HashCombine(h, v.RTDesc);
		Mist::HashCombine(h, v.BlendState);
		return h;
	}
};

namespace Mist
{
	class Swapchain;

	class Renderer
	{
	public:
		struct tCopyParams
		{
			const RenderContext* Context = nullptr;
			const RenderTarget* Src = nullptr;
			RenderTarget* Dst = nullptr;
			tColorBlendState BlendState;
		};

		void Init(const RenderContext& context, RenderFrameContext* frameContextArray, uint32_t frameContextCount, const Swapchain& swapchain);
		void Destroy(const RenderContext& context);
		void UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext);
		void Draw(const RenderContext& context, const RenderFrameContext& frameContext);
		void ImGuiDraw();

		const RenderProcess* GetRenderProcess(RenderProcessType type) const;
		RenderProcess* GetRenderProcess(RenderProcessType type);

		const RenderTarget& GetPresentRenderTarget(uint32_t index) const;
		RenderTarget& GetPresentRenderTarget(uint32_t index);

		const RenderTarget& GetLDRTarget() const { return *m_ldr; }
		RenderTarget& GetLDRTarget() { return *m_ldr; }
		void CopyRenderTarget(const tCopyParams& params);

	private:
		class RenderProcess* m_processArray[RENDERPROCESS_COUNT];
		tStaticArray<RenderTarget*, globals::MaxOverlappedFrames> m_presentRenderTargets;
		ShaderProgram* m_copyProgram;
		tMap<tCopyShaderKey, ShaderProgram*> m_copyPrograms;
		RenderTarget* m_ldr;
	};
}


