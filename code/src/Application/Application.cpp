#include "Application/Application.h"
#include "Core/Debug.h"
#include "Render/VulkanRenderEngine.h"
#include "Core/Logger.h"
#include "Application/CmdParser.h"

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

        logfinfo("Cmd line: %s", argv[0]);
        for (int i = 1; i < argc; ++i)
        {
            logfinfo(" %s", argv[i]);
            char* it = strstr(argv[i], "-");
            if (it)
            {
                char* endVar = strstr(it+1, ":");
                check(endVar);
                size_t varLen = static_cast<size_t>(endVar - (it + 1));
                char varName[64];
                check(varLen < 64ul);
                strncpy_s(varName, it + 1, varLen);
                CVar* cvar = FindCVar(varName);
                if (!cvar)
                    continue;
                switch (cvar->GetType())
                {
                case CVar::CVarType::Int:
                {
                    int value = atoi(endVar + 1);
                    CIntVar* intVar = static_cast<CIntVar*>(cvar);
                    intVar->Set(value);
                }
                    break;
                case CVar::CVarType::Float:
				{
					float value = atof(endVar + 1);
					CFloatVar* floatVar = static_cast<CFloatVar*>(cvar);
                    floatVar->Set(value);
				}
                    break;
                case CVar::CVarType::Bool:
				{
                    bool value = !strcmp(endVar + 1, "true") || atoi(endVar+1) > 0;
					CBoolVar* boolVar = static_cast<CBoolVar*>(cvar);
                    boolVar->Set(value);
				}
                    break;
                case CVar::CVarType::String:
				{
                    const char* str = endVar + 1;
					CStrVar* strVar = static_cast<CStrVar*>(cvar);
                    strVar->Set(str);
				}
                    break;
                default:
                    check(false);
                    break;
                }
            }
        }
        loginfo("\n");

        m_window = Window::Create(1920, 1080, "MistEngine");
        m_engine = IRenderEngine::MakeInstance();
        m_engine->Init(m_window);
    }

    void tApplication::Destroy()
    {
        m_engine->Shutdown();
        IRenderEngine::FreeRenderEngine();
        m_engine = nullptr;
        Window::Destroy(m_window);
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
