#include <cstdint>

#include <vkmmc/RenderEngine.h>
#include <vkmmc/RenderObject.h>
#include <vkmmc/Mesh.h>
#include <vkmmc/SceneLoader.h>

#include <chrono>
#include <corecrt_math_defines.h>

#include <imgui/imgui.h>

class Timer
{
public:
	Timer() : m_start(Now()) {}

	std::chrono::high_resolution_clock::time_point Now() const 
	{
		return std::chrono::high_resolution_clock::now();
	}

	float ElapsedNow() const 
	{
		auto n = Now();
		auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(n - m_start);
		float s = (float)diff.count() * 1e-9f;
		return s;
	}

private:
	std::chrono::high_resolution_clock::time_point m_start;
};

Timer GTimer;
bool GIsPaused = true;
float GAmplitude = 2.f;
float GFrequency = 1.f; // Hz
float GAxisXPhase = 0.2f;
float GAxisZPhase = 0.2f;
float GNoise = 0.02f;

void ProcessLogic(vkmmc::IRenderEngine* engine, const Timer& timer)
{
	if (!GIsPaused)
	{
		float time = timer.ElapsedNow();
		for (uint32_t i = 0; i < engine->GetObjectCount(); ++i)
		{
			vkmmc::RenderObjectTransform& t = *engine->GetObjectTransform({ i });
			int32_t r = std::rand();
			float noise = GNoise * ((float)r / (float)RAND_MAX);
			float rotY = GAmplitude * sinf(GFrequency * 2.f * (float)M_PI * time + t.Position.x * GAxisXPhase + t.Position.z * GAxisZPhase) + noise;
			//t.Rotation.y += rotY;
			t.Position.y = rotY;
		}
	}
}

void SpawnMeshGrid(vkmmc::IRenderEngine* engine, vkmmc::Mesh& mesh, vkmmc::Material& mtl, const glm::ivec3& gridDim, const glm::vec3& cellSize)
{
	for (int32_t x = -gridDim[0] / 2; x < gridDim[0] / 2; ++x)
	{
		for (int32_t z = -gridDim[2] / 2; z < gridDim[2] / 2; ++z)
		{
			for (int32_t y = -gridDim[1] / 2; y < gridDim[1] / 2; ++y)
			{
				vkmmc::RenderObject obj = engine->NewRenderObject();
				vkmmc::RenderObjectTransform& t = *engine->GetObjectTransform(obj);
				t.Position = { (float)x * cellSize.x, (float)y * cellSize.y, (float)z * cellSize.z };
				vkmmc::RenderObjectMesh& m = *engine->GetObjectMesh(obj);
				m.StaticMesh = mesh;
				m.Mtl = mtl;
			}
		}
	}

	engine->SetImGuiCallback([]() 
		{
			ImGui::Begin("Magic params");
			ImGui::Text("Time app: %.4f s", GTimer.ElapsedNow());
			ImGui::Checkbox("Pause", &GIsPaused);
			ImGui::DragFloat("Amplitude", &GAmplitude, 0.2f);
			ImGui::DragFloat("Frequency (Hz)", &GFrequency, 0.02f);
			float phase[] = { GAxisXPhase, GAxisZPhase };
			if (ImGui::DragFloat2("Phase", phase, 0.1f))
			{
				GAxisXPhase = phase[0];
				GAxisZPhase = phase[1];
			}
			ImGui::DragFloat("Noise", &GNoise, 0.01f);
			ImGui::End();
		});
}

void ExecuteSponza(vkmmc::IRenderEngine* engine)
{
	vkmmc::Scene scene = vkmmc::SceneLoader::LoadScene(engine, "../../assets/models/sponza/Sponza.gltf");

	for (uint32_t i = 0; i < scene.Meshes.size(); ++i)
	{
		const vkmmc::SceneNodeMesh& node = scene.Meshes[i];
		for (uint32_t j = 0; j < node.Meshes.size(); ++j)
		{
			vkmmc::RenderObject object = engine->NewRenderObject();
			vkmmc::RenderObjectMesh* rom = engine->GetObjectMesh(object);
			rom->StaticMesh = node.Meshes[j];
			rom->Mtl = node.Materials[j];
		}
	}
}

void ExecuteBoxBiArray(vkmmc::IRenderEngine* engine)
{
	vkmmc::Scene scene = vkmmc::SceneLoader::LoadScene(engine, "../../assets/models/box/Box.gltf");
	vkmmc::Mesh mesh = scene.Meshes[0].Meshes[0];
	engine->UploadMesh(mesh);

	vkmmc::Material material;
	engine->UploadMaterial(material);

#ifdef _DEBUG
	SpawnMeshGrid(engine, mesh, material, glm::ivec3{ 20, 2, 20 }, glm::vec3{ 2.f });
#else
	SpawnMeshGrid(engine, mesh, material, glm::ivec3{ 120, 2, 120 }, glm::vec3{ 2.f });
#endif
}

int main(int32_t argc, char** argv)
{
	vkmmc::InitializationSpecs spec
	{
		1920, 1080, "VkMMC Engine"
	};
	vkmmc::IRenderEngine* engine = vkmmc::NewRenderEngine();
	engine->Init(spec);

	ExecuteSponza(engine);

	bool terminate = false;
	while (!terminate)
	{
		ProcessLogic(engine, GTimer);
		terminate = !engine->RenderProcess();
	}

	engine->Shutdown();
	vkmmc::FreeRenderEngine(&engine);
	return 0;
}