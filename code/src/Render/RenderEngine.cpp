
#include "Render/VulkanRenderEngine.h"
#include "Core/SystemMemory.h"

namespace Mist
{
	static IRenderEngine* GRenderEngine = nullptr;

	IRenderEngine* IRenderEngine::MakeInstance()
	{
		check(!GRenderEngine);
		GRenderEngine = _new VulkanRenderEngine();
		return GRenderEngine;
	}

	IRenderEngine* IRenderEngine::GetRenderEngine()
	{
		check(GRenderEngine);
		return GRenderEngine;
	}

	void IRenderEngine::FreeRenderEngine()
	{
		check(GRenderEngine);
		delete GRenderEngine;
		GRenderEngine = nullptr;
	}
}