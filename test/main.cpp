#include <cstdint>

#include <vkmmc/RenderEngine.h>
#include <vkmmc/Mesh.h>

int main(uint32_t argc, char** argv)
{
	vkmmc::InitializationSpecs spec
	{
		1920, 1080, "VkMMC Engine"
	};
	vkmmc::IRenderEngine* engine = vkmmc::NewRenderEngine();
	engine->Init(spec);
	engine->RenderLoop();
	engine->Shutdown();
	vkmmc::FreeRenderEngine(&engine);
	return 0;
}