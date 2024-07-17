#include "Application.h"
#include "Debug.h"
#include "VulkanRenderEngine.h"

extern Mist::tApplication* CreateGameApplication();
extern void DestroyGameApplication(Mist::tApplication*);

namespace Mist
{
    tApplication* tApplication::CreateApplication(int argc, char** argv)
    {
        tApplication* app = CreateGameApplication();
        app->Init(argc, argv);
        return app;
    }

    void tApplication::DestroyApplication(tApplication* app)
    {
        app->Destroy();
        DestroyGameApplication(app);
    }

    void tApplication::Init(int argc, char** argv)
    {
        check(!m_engine);
        m_engine = IRenderEngine::MakeInstance();
        m_engine->Init({ 1920, 1080, "MistEngine" });
    }

    void tApplication::Destroy()
    {
        m_engine->Shutdown();
        IRenderEngine::FreeRenderEngine();
        m_engine = nullptr;
    }

    int tApplication::Run()
    {
        int result = 0;
        while (!result)
        {
            LogicProcess(0.033f);
            if (!m_engine->RenderProcess())
                result = EXIT_FAILURE;
        }
        return result;
    }
}
