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
	typedef std::chrono::high_resolution_clock::time_point TimePoint;

	Timer() : m_start(Now()) {}

	void Update()
	{
		m_mark = Now();
	}

	static std::chrono::high_resolution_clock::time_point Now()
	{
		return std::chrono::high_resolution_clock::now();
	}

	static float ElapsedFrom(TimePoint point)
	{
		auto n = Now();
		auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(n - point);
		float s = (float)diff.count() * 1e-9f;
		return s;
	}

	float ElapsedNow() const
	{
		return ElapsedFrom(m_mark);
	}

	float ElapsedFromStart() const
	{
		return ElapsedFrom(m_start);
	}

private:
	TimePoint m_start;
	TimePoint m_mark;
};

class Test
{
public:
	virtual ~Test() {};
	void Init()
	{
		vkmmc::InitializationSpecs spec
		{
			1920, 1080, "VkMMC Engine"
		};
		m_engine = vkmmc::IRenderEngine::MakeInstance();
		m_engine->Init(spec);

		m_engine->AddImGuiCallback([this]() { m_camera.ImGuiDraw(); });
		m_engine->SetAppEventCallback([this](void* d) { m_camera.ProcessEvent(d); ProcessEvent(d); });

		LoadTest();
	}

	void RunLoop()
	{
		bool terminate = false;
		while (!terminate)
		{
			m_timer.Update();
			m_camera.Tick(0.033f);
			ProcessLogic(0.033f); // TODO: better time control.
			m_engine->UpdateSceneView(m_camera.GetCamera().GetView(), m_camera.GetCamera().GetProjection());
			terminate = !m_engine->RenderProcess();
		}
	}

	void Destroy()
	{
		UnloadTest();
		m_engine->Shutdown();
		vkmmc::IRenderEngine::FreeRenderEngine();
	}

protected:
	virtual void LoadTest() {}
	virtual void ProcessLogic(float timeDiff) {}
	virtual void UnloadTest() {}
	virtual void ProcessEvent(void* eventData) {}

protected:
	vkmmc::IRenderEngine* m_engine = nullptr;
	vkmmc::CameraController m_camera;
	Timer m_timer;
};

class SponzaTest : public Test
{
protected:
	virtual void LoadTest()
	{
		vkmmc::IScene* scene = vkmmc::IScene::LoadScene(m_engine, "assets/models/sponza/Sponza.gltf");
		scene->LoadModel("assets/models/vulkanscene_shadow.gltf");
		m_engine->SetScene(scene);
	}
};

Test* CreateTest(int32_t argc, char** argv)
{
	// TODO: read cmd args.
	return new SponzaTest();
}


int main(int32_t argc, char** argv)
{
	Test* test = CreateTest(argc, argv);
	test->Init();
	test->RunLoop();
	test->Destroy();
	delete test;
	return 0;
}