#include <cstdint>

#include <Render/VulkanRenderEngine.h>
#include <Scene/Scene.h>

#include <chrono>
#include <corecrt_math_defines.h>

#include <imgui/imgui.h>
#include "Render/Camera.h"
#include "Core/Logger.h"
#include "Core/SystemMemory.h"
#include "glm/gtx/transform.hpp"
#include "Application/Application.h"
#include "Resources/ResourceSystem.h"
#include "RenderSystem/UI.h"

Mist::CStrVar CVar_LoadScene("g_LoadScene", "scenes/empty.yaml");


static const char* g_scenes[] =
{
	"scenes/Emissive.yaml",
	"scenes/Empty.yaml",
	"scenes/lights_on.yaml",
	"scenes/pirates.yaml",
	"scenes/bistro.yaml",
	"scenes/Scene.yaml",
	"scenes/shadowmap.yaml",
	"scenes/Sponza.yaml",
	"scenes/vkscene.yaml",
};

class tGameApplication : public Mist::tApplication
{
public:

	virtual void Init(int argc, char** argv) override
	{
		Mist::tApplication::Init(argc, argv);
		
		check(!m_engine);
		m_engine = (Mist::VulkanRenderEngine*)GetEngineInstance();
		Mist::Scene* scene = new Mist::Scene(m_engine);
		m_engine->SetScene(scene);
		//scene->LoadScene(CVar_LoadScene.Get());
		//FixSceneFiles(scene);

		rendersystem::ui::AddMenuCallback("Scenes", [](void* userData) 
			{
				tGameApplication* app = static_cast<tGameApplication*>(userData);
				if (ImGui::BeginMenu("Scenes"))
				{
					if (ImGui::Button("Unload current"))
						app->m_engine->GetScene()->Destroy();

					ImGui::Separator();
					for (uint32_t i = 0; i < Mist::CountOf(g_scenes); ++i)
					{
						char buff[64];
						sprintf_s(buff, "%s", g_scenes[i]);
						if (ImGui::Button(buff))
						{
							app->m_engine->GetScene()->Destroy();
							app->m_engine->GetScene()->LoadScene(buff);
						}
					}

					ImGui::EndMenu();
				}
			}, this);
	}

protected:

	void FixSceneFiles(Mist::Scene* scene)
	{

		Mist::resource_system::PushInmediateMode(true);

		for (uint32_t i = 0; i < Mist::CountOf(g_scenes); ++i)
		{
			logfok("fix scene: %s\n", g_scenes[i]);
			Mist::memory::DumpMemoryStats();
			scene->LoadScene(g_scenes[i]);
			scene->SaveScene(g_scenes[i]);
			scene->Destroy();
			Mist::g_device->PurgeGarbageCollector();
			Mist::memory::DumpMemoryStats();
		}

		Mist::resource_system::PopInmediateMode();
	}

	virtual void ProcessImGui() override
	{

	}
private:
	Mist::VulkanRenderEngine* m_engine = nullptr;
};

Mist::tApplication* CreateGameApplication()
{
	return _new tGameApplication();
}

void DestroyGameApplication(Mist::tApplication* app)
{
	delete app;
}
