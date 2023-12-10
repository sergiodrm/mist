#include <cstdint>

#include <vkmmc/RenderEngine.h>
#include <vkmmc/RenderObject.h>
#include <vkmmc/Mesh.h>

int main(int32_t argc, char** argv)
{
	vkmmc::InitializationSpecs spec
	{
		1920, 1080, "VkMMC Engine"
	};
	vkmmc::IRenderEngine* engine = vkmmc::NewRenderEngine();
	engine->Init(spec);


	vkmmc::Mesh mesh;
	const vkmmc::Vertex vertices[] = {
		{{-1.f, 1.f, 0.f}},
		{{1.f, 1.f, 0.f}},
		{{1.f, -1.f, 0.f}},

		{{1.f, -1.f, 0.f}},
		{{-1.f, -1.f, 0.f}},
		{{-1.f, 1.f, 0.f}},
	};
	mesh.SetVertices(vertices, sizeof(vertices) / sizeof(vkmmc::Vertex));
	engine->UploadMesh(mesh);

	constexpr int32_t dim[2] = { 20, 20 };
	constexpr float dif = 10.f;
	for (int32_t x = -dim[0]; x <= dim[0]; ++x)
	{
		for (int32_t y = -dim[1]; y <= dim[1]; ++y)
		{
			vkmmc::RenderObject object;
			object.m_mesh = mesh;
			object.m_position = { (float)x * dif, 0.f, (float)y * dif };
			engine->AddRenderObject(object);
		}
	}

	engine->RenderLoop();
	engine->Shutdown();
	vkmmc::FreeRenderEngine(&engine);
	return 0;
}