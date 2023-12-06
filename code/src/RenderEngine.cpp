
#include "VulkanRenderEngine.h"

namespace vkmmc
{
	IRenderEngine* NewRenderEngine()
	{
		return new VulkanRenderEngine();
	}

	void FreeRenderEngine(IRenderEngine** engine)
	{
		delete* engine;
		*engine = nullptr;
	}
}