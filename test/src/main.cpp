#include <cstdint>

#include <Mist/RenderEngine.h>
#include <Mist/RenderObject.h>
#include <Mist/Mesh.h>
#include <Mist/Scene.h>

#include <chrono>
#include <corecrt_math_defines.h>

#include <imgui/imgui.h>
#include "Mist/Camera.h"
#include "Mist/Logger.h"
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
	bool Init()
	{
		Mist::InitializationSpecs spec
		{
			1920, 1080, "MistEngine"
		};
		m_engine = Mist::IRenderEngine::MakeInstance();
		m_engine->Init(spec);

		m_engine->AddImGuiCallback([this]() { m_camera.ImGuiDraw(); });
		m_engine->SetAppEventCallback([this](void* d) { m_camera.ProcessEvent(d); ProcessEvent(d); });

		return LoadTest();
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
		Mist::IRenderEngine::FreeRenderEngine();
	}

protected:
	virtual bool LoadTest() { return true; }
	virtual void ProcessLogic(float timeDiff) {}
	virtual void UnloadTest() {}
	virtual void ProcessEvent(void* eventData) {}

protected:
	Mist::IRenderEngine* m_engine = nullptr;
	Mist::CameraController m_camera;
	Timer m_timer;
};

class SponzaTest : public Test
{
protected:
	virtual bool LoadTest()
	{
		const char* scenePath = "../assets/models/sponza/Sponza.gltf";
		Mist::IScene* scene = Mist::IScene::LoadScene(m_engine, scenePath);
		//scene->LoadModel("assets/models/vulkanscene_shadow.gltf");
		if (!scene)
			Mist::Logf(Mist::LogLevel::Error, "Error loading test scene: %s\n", scenePath);
		return scene;
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
	if (test->Init())
	{
		test->RunLoop();
	}
	test->Destroy();
	delete test;
	return 0;
}