#include <cstdint>

#include <Render/VulkanRenderEngine.h>
#include <Render/Mesh.h>
#include <Scene/Scene.h>

#include <chrono>
#include <corecrt_math_defines.h>

#include <imgui/imgui.h>
#include "Render/Camera.h"
#include "Core/Logger.h"
#include "Core/SystemMemory.h"
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
#if 0

class Test
{
public:
	virtual ~Test() {};
	bool Init()
	{
		Mist::InitializationSpecs spec
		{
			1920, 1080, "MistEngine"
			//1080, 720, "MistEngine"
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
	virtual void ProcessLogic(float timeDiff) {	}
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
#define TEST_LOAD
#ifdef TEST_LOAD
		scene = Mist::IScene::LoadScene(m_engine, "../assets/scenes/scene.yaml");
#else
		//const char* scenePath = "../assets/models/vulkanscene_shadow.gltf";
		const char* scenePath = "../assets/models/sponza/sponzatest/Sponzatest.gltf";
		Mist::IScene* scene = Mist::IScene::LoadScene(m_engine, scenePath);
		//scene->LoadModel("assets/models/vulkanscene_shadow.gltf");  
		if (!scene)
		{
			Mist::Logf(Mist::LogLevel::Error, "Error loading test scene: %s\n", scenePath);
			return false;
	}
#endif // TEST_LOAD


#ifndef TEST_LOAD
		Mist::RenderObject obj = scene->CreateRenderObject(scene->GetRoot());
		Mist::LightComponent light;
		light.Type = Mist::ELightType::Spot;
		light.Color = { 1.f, 0.f, 0.f };
		light.InnerCutoff = 1.f;
		light.OuterCutoff = 0.5f;
		scene->SetLight(obj, light);
		scene->SetRenderObjectName(obj, "Light");
#endif // TEST_LOAD

		return scene != nullptr;
}

	virtual void ProcessLogic(float timeDiff)
	{
#if 0
		static float accumulated = timeDiff;
		Mist::RenderObject lightObject;
		for (uint32_t i = 0; i < scene->GetRenderObjectCount(); ++i)
		{
			if (!strcmp(scene->GetRenderObjectName(i), "Light"))
			{
				lightObject = i;
				break;
			}
		}
		if (lightObject.IsValid())
		{
			Mist::TransformComponent transform = scene->GetTransform(lightObject);
			float speed = 1.f;
			float amplitude = 10.f;
			float freq = 0.2f;
			float movement = amplitude * sin(accumulated * freq * 2.f * M_PI) * speed * timeDiff;
			transform.Position.x = movement;
			movement = amplitude * cos(accumulated * freq * 2.f * M_PI) * speed * timeDiff;
			transform.Position.z = movement;
			accumulated += timeDiff;
			scene->SetTransform(lightObject, transform);
		}
#endif // 0

	}

	Mist::IScene* scene = nullptr;
};

Test* CreateTest(int32_t argc, char** argv)
{
	// TODO: read cmd args.
	return _new SponzaTest();
}

void DestroyTest(Test* test)
{
	delete test;
}


int main(int32_t argc, char** argv)
{
	Test* test = CreateTest(argc, argv);
	if (test->Init())
	{
		test->RunLoop();
	}
	test->Destroy();
	DestroyTest(test);
	return 0;
}
#endif // 0

#include "Application/Application.h"

Mist::CStrVar CVar_LoadScene("g_LoadScene", "../assets/scenes/empty.yaml");

class tGameApplication : public Mist::tApplication
{
public:

	virtual void Init(int argc, char** argv) override
	{
		Mist::tApplication::Init(argc, argv);
		
		Mist::VulkanRenderEngine* engine = (Mist::VulkanRenderEngine*)GetEngineInstance();
		Mist::Scene* scene = new Mist::Scene(engine);
		engine->SetScene(scene);
		scene->LoadScene(CVar_LoadScene.Get());
	}

protected:
private:
};

Mist::tApplication* CreateGameApplication()
{
	return _new tGameApplication();
}

void DestroyGameApplication(Mist::tApplication* app)
{
	delete app;
}
