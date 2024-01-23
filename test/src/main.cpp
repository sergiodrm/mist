#include <cstdint>

#include <vkmmc/RenderEngine.h>
#include <vkmmc/RenderObject.h>
#include <vkmmc/Mesh.h>
#include <vkmmc/Scene.h>

#include <chrono>
#include <corecrt_math_defines.h>

#include <imgui/imgui.h>
#include "vkmmc/Camera.h"
#include "glm/gtx/transform.hpp"

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
		vkmmc::IScene& scene = *engine->GetScene();
		for (uint32_t i = 0; i < scene.GetRenderObjectCount(); ++i)
		{
			glm::mat4 transform = scene.GetTransform(i);
			glm::vec3 pos = transform[3];
			int32_t r = std::rand();
			float noise = GNoise * ((float)r / (float)RAND_MAX);
			float rotY = GAmplitude * sinf(GFrequency * 2.f * (float)M_PI * time + pos.x * GAxisXPhase + pos.z * GAxisZPhase) + noise;
			pos.y = rotY;
			glm::mat4 newTransform = glm::translate(transform, pos);
			scene.SetTransform(i, newTransform);
		}
	}
}

void SpawnMeshGrid(vkmmc::IRenderEngine* engine, vkmmc::Mesh& mesh, vkmmc::Material& mtl, const glm::ivec3& gridDim, const glm::vec3& cellSize)
{
	vkmmc::IScene& scene = *engine->GetScene();
	vkmmc::RenderObject root = scene.CreateRenderObject(vkmmc::RenderObject::InvalidId);
	vkmmc::Model model;
	model.m_meshArray.push_back(mesh);
	model.m_materialArray.push_back(mtl);
	for (int32_t x = -gridDim[0] / 2; x < gridDim[0] / 2; ++x)
	{
		for (int32_t z = -gridDim[2] / 2; z < gridDim[2] / 2; ++z)
		{
			for (int32_t y = -gridDim[1] / 2; y < gridDim[1] / 2; ++y)
			{
				vkmmc::RenderObject obj = scene.CreateRenderObject(root);
				glm::vec3 pos = { (float)x * cellSize.x, (float)y * cellSize.y, (float)z * cellSize.z };
				scene.SetModel(obj, model);
				scene.SetTransform(obj, glm::translate(glm::mat4(1.f), pos));
			}
		}
	}

	engine->AddImGuiCallback([]() 
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
	vkmmc::IScene* scene = vkmmc::IScene::LoadScene(engine, "../../assets/models/sponza/Sponza.gltf");
	engine->SetScene(scene);
}

void ExecuteBoxBiArray(vkmmc::IRenderEngine* engine)
{
	vkmmc::IScene* scene = vkmmc::IScene::LoadScene(engine, "../../assets/models/box/Box.gltf");
	vkmmc::Mesh mesh;
	vkmmc::Material material;
	for (uint32_t i = 0; i < scene->GetRenderObjectCount(); ++i)
	{
		const vkmmc::Model* model = scene->GetModel(i);
		mesh = model ? model->m_meshArray[0] : vkmmc::Mesh();
		material = model ? model->m_materialArray[0] : vkmmc::Material();
		if (mesh.GetHandle().IsValid())
			break;
	}

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
	vkmmc::IRenderEngine* engine = vkmmc::IRenderEngine::MakeInstance();
	engine->Init(spec);

	ExecuteSponza(engine);

	vkmmc::CameraController cameraController;
	engine->AddImGuiCallback([&cameraController]() 
		{
			cameraController.ImGuiDraw();
		});

	bool terminate = false;
	while (!terminate)
	{
		cameraController.Tick(0.033f);
		ProcessLogic(engine, GTimer);
		engine->UpdateSceneView(cameraController.GetCamera().GetView(), cameraController.GetCamera().GetProjection());
		terminate = !engine->RenderProcess();
	}
	engine->Shutdown();
	vkmmc::IRenderEngine::FreeRenderEngine();
	return 0;
}