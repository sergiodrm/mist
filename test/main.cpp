#include <cstdint>

#include <vkmmc/RenderEngine.h>
#include <vkmmc/RenderObject.h>
#include <vkmmc/Mesh.h>

int main(uint32_t argc, char** argv)
{
	vkmmc::InitializationSpecs spec
	{
		1920, 1080, "VkMMC Engine"
	};
	vkmmc::IRenderEngine* engine = vkmmc::NewRenderEngine();
	engine->Init(spec);


	vkmmc::Mesh mesh;
	const vkmmc::Vertex vertices[] = {
		{ {-1.f, 1.f, 0.f} },
		{ {1.f, 1.f, 0.f} },
		{ {0.f, -1.f, 0.f} }
	};
	mesh.SetVertices(vertices, 3);
	engine->UploadMesh(mesh);
	vkmmc::RenderObject object;
	object.m_mesh = mesh;
	engine->AddRenderObject(object);
	engine->RenderLoop();
	engine->Shutdown();
	vkmmc::FreeRenderEngine(&engine);
	return 0;
}