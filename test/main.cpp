#include <cstdint>

#include <vkmmc/RenderEngine.h>
#include <vkmmc/RenderObject.h>
#include <vkmmc/Mesh.h>

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

float GAmplitude = 2.f;
float GFrequency = 1.f; // Hz
float GAxisXPhase = 0.2f;
float GAxisZPhase = 0.2f;
float GNoise = 0.02f;

void ProcessLogic(vkmmc::IRenderEngine* engine, const Timer& timer)
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

void ImGuiLogic()
{
	ImGui::Begin("Magic params");
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
	engine->SetImGuiCallback(&ImGuiLogic);

	constexpr int32_t dim[2] = { 120, 120 };
	constexpr float dif = 2.f;
	for (int32_t x = -dim[0] / 2; x < dim[0] / 2; ++x)
	{
		for (int32_t y = -dim[1] / 2; y < dim[1] / 2; ++y)
		{
			vkmmc::RenderObject obj = engine->NewRenderObject();
			vkmmc::RenderObjectTransform& t = *engine->GetObjectTransform(obj);
			t.Position = { (float)x * dif, 0.f, (float)y * dif };
			vkmmc::RenderObjectMesh& m = *engine->GetObjectMesh(obj);
			m.StaticMesh = mesh;
			//t.Rotation = { glm::radians(90.f), 0.f, 0.f };
		}
	}

	Timer timer;
	bool terminate = false;
	while (!terminate)
	{
		ProcessLogic(engine, timer);
		terminate = !engine->RenderProcess();
	}

	engine->Shutdown();
	vkmmc::FreeRenderEngine(&engine);
	return 0;
}