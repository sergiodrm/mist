
#include "VulkanRenderEngine.h"

namespace vkmmc
{
	IRenderEngine* IRenderEngine::NewRenderEngine()
	{
		return new VulkanRenderEngine();
	}

	void IRenderEngine::FreeRenderEngine(IRenderEngine** engine)
	{
		delete* engine;
		*engine = nullptr;
	}
}