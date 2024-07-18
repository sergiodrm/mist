
#include "Render/VulkanRenderEngine.h"

namespace Mist
{
	static IRenderEngine* GRenderEngine = nullptr;

	IRenderEngine* IRenderEngine::MakeInstance()
	{
		check(!GRenderEngine);
		GRenderEngine = new VulkanRenderEngine();
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