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

Mist::CStrVar CVar_LoadScene("g_LoadScene", "");

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
		if (CVar_LoadScene.Get() && *CVar_LoadScene.Get())
			scene->LoadScene(CVar_LoadScene.Get());

		Mist::FileSystem::FileListDir(m_sceneFiles, &m_sceneCount, "scenes/");
		check(m_sceneFiles && m_sceneCount);

		rendersystem::ui::AddMenuCallback("Scenes", [](void* userData) 
			{
				tGameApplication* app = static_cast<tGameApplication*>(userData);
				if (ImGui::BeginMenu("Scenes"))
				{
					if (ImGui::Button("Unload current"))
						app->m_engine->GetScene()->Destroy();
					if (ImGui::Button("Save current"))
						app->m_engine->GetScene()->SaveScene();
					if (ImGui::Button("Fix scenes"))
						app->FixSceneFiles(app->m_engine->GetScene());

					ImGui::SeparatorText("Load scene");
					for (uint32_t i = 0; i < app->m_sceneCount; ++i)
					{
						char buff[64];
						sprintf_s(buff, "%s", app->m_sceneFiles[i]);
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

	virtual void Destroy()
	{
		for (uint32_t i = 0; i < m_sceneCount; ++i)
			delete[] m_sceneFiles[i];
		delete[] m_sceneFiles;
	}

protected:

	void FixSceneFiles(Mist::Scene* scene)
	{
		Mist::resource_system::PushInmediateMode(true);

		for (uint32_t i = 0; i < m_sceneCount; ++i)
		{
			logfok("fix scene: %s\n", m_sceneFiles[i]);
			Mist::memory::DumpMemoryStats();
			scene->LoadScene(m_sceneFiles[i]);
			scene->SaveScene(m_sceneFiles[i]);
			scene->Destroy();
			Mist::GetRenderDevice()->PurgeGarbageCollector();
			Mist::memory::DumpMemoryStats();
		}

		Mist::resource_system::PopInmediateMode();
	}
private:
	Mist::VulkanRenderEngine* m_engine = nullptr;

	char** m_sceneFiles = nullptr;
	uint32_t m_sceneCount = 0;
};

Mist::tApplication* CreateGameApplication()
{
	return _new tGameApplication();
}

void DestroyGameApplication(Mist::tApplication* app)
{
	delete app;
}
