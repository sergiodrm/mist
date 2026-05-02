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

Mist::CStrVar CVar_LoadScene("g_LoadScene", "scenes/empty.yaml");

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
