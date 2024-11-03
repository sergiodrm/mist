#include "Application/Application.h"
#include "Core/Debug.h"
#include "Render/VulkanRenderEngine.h"
#include "Core/Logger.h"
#include "Application/CmdParser.h"
#include "Utils/FileSystem.h"

extern Mist::tApplication* CreateGameApplication();
extern void DestroyGameApplication(Mist::tApplication*);


namespace Mist
{
    CStrVar GIniFile("IniFile", "default.ini");

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
                SetCVar(varName, endVar + 1);
            }
        }
        loginfo("\n");

        cIniFile iniFile(GIniFile.Get());

        int w; 
        int h;
        iniFile.GetInt("width", w, 1920);
        iniFile.GetInt("height", h, 1080);

        for (index_t i = 0; i < iniFile.GetValueCount(); ++i)
        {
            if (SetCVar(iniFile.GetKey(i), iniFile.GetValue(i)))
                logfinfo("CVar setted from %s file: %s = %s\n", GIniFile.Get(), iniFile.GetKey(i), iniFile.GetValue(i));
        }



        m_window = Window::Create(w, h, "MistEngine");
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
