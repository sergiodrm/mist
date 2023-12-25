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

void ImGuiLogic()
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
}

int main(int32_t argc, char** argv)
{
	vkmmc::InitializationSpecs spec
	{
		1920, 1080, "VkMMC Engine"
	};
	vkmmc::IRenderEngine* engine = vkmmc::NewRenderEngine();
	engine->Init(spec);
	engine->SetImGuiCallback(&ImGuiLogic);

	vkmmc::Scene scene = vkmmc::SceneLoader::LoadScene("../../assets/models/box/Box.gltf");
	for (uint32_t i = 0; i < scene.Meshes.size(); ++i)
	{
		for (vkmmc::Mesh& mesh : scene.Meshes[i].Meshes)
		{
			engine->UploadMesh(mesh);
		}
	}

	vkmmc::RenderHandle marioTexHandle = engine->LoadTexture("../../assets/textures/test.png");
	vkmmc::RenderHandle luigiTexHandle = engine->LoadTexture("../../assets/textures/luigi.png");
	vkmmc::RenderHandle peachTexHandle = engine->LoadTexture("../../assets/textures/peach.png");

	vkmmc::Material material;
	material.SetDiffuseTexture(marioTexHandle);
	material.SetNormalTexture(luigiTexHandle);
	material.SetSpecularTexture(peachTexHandle);
	engine->UploadMaterial(material);


#ifdef _DEBUG
	constexpr int32_t dim[2] = { 12, 12 };
#else
	constexpr int32_t dim[2] = { 120, 120 };
#endif
	constexpr float dif = 2.f;
	for (int32_t x = -dim[0] / 2; x < dim[0] / 2; ++x)
	{
		for (int32_t y = -dim[1] / 2; y < dim[1] / 2; ++y)
		{
			vkmmc::RenderObject obj = engine->NewRenderObject();
			vkmmc::RenderObjectTransform& t = *engine->GetObjectTransform(obj);
			t.Position = { (float)x * dif, 0.f, (float)y * dif };
			vkmmc::RenderObjectMesh& m = *engine->GetObjectMesh(obj);
			m.StaticMesh = scene.Meshes[0].Meshes[0];
			m.Mtl = material;
		}
	}

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