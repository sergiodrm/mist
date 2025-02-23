#include "Application/Application.h"
#include "Core/Debug.h"
#include "Render/VulkanRenderEngine.h"
#include "Core/Logger.h"
#include "Application/CmdParser.h"
#include "Utils/FileSystem.h"
#include "Event.h"
#include "SDL_events.h"

extern Mist::tApplication* CreateGameApplication();
extern void DestroyGameApplication(Mist::tApplication*);


namespace Mist
{
	extern CIntVar CVar_ShowConsole;
	extern CIntVar CVar_ShowStats;
	extern CBoolVar CVar_ShowImGui;

	CStrVar GIniFile("IniFile", "default.cfg");

	uint64_t GFrame = 0;

	SDL_WindowFlags WindowFlagsToSDL(eWindowFlags f)
	{
		uint32_t r = 0;
		if (f & WindowFlags_Borderless) r |= SDL_WINDOW_BORDERLESS;
		if (f & WindowFlags_Fullscreen) r |= SDL_WINDOW_FULLSCREEN;
		if (f & WindowFlags_Shown) r |= SDL_WINDOW_SHOWN;
		if (f & WindowFlags_Hidden) r |= SDL_WINDOW_HIDDEN;
		if (f & WindowFlags_Resizable) r |= SDL_WINDOW_RESIZABLE;
		return (SDL_WindowFlags)r;
	}

	Window Window::Create(uint32_t width, uint32_t height, uint32_t posx, uint32_t posy, const char* title, eWindowFlags flags)
	{
		Window newWindow;
		newWindow.Width = width;
		newWindow.Height = height;
		strcpy_s(newWindow.Title, title);

		if (SDL_Init(SDL_INIT_VIDEO) != 0)
		{
			const char* sdlError = SDL_GetError();
			logferror("Error initializating SDL. Error msg:\n%s\n", sdlError);
			check(false && "Failed initializating SDL.");
		}
		int numDisplays = SDL_GetNumVideoDisplays();
		logfinfo("Num displays detected: %d\n", numDisplays);
		for (int i = 0; i < numDisplays; ++i)
		{
			SDL_Rect rect;
			SDL_GetDisplayBounds(i, &rect);
			const char* displayName = SDL_GetDisplayName(i);
			logfinfo("#%02d %16s (%4dx%4d | %5d,%5d)\n", i, displayName, rect.w, rect.h, rect.x, rect.y);
		}

		SDL_WindowFlags windowFlags = (SDL_WindowFlags)(uint32_t(WindowFlagsToSDL(flags)) | SDL_WINDOW_VULKAN);
		newWindow.WindowInstance = SDL_CreateWindow(
			title, posx, posy,
			width, height, windowFlags);
		return newWindow;
	}

	void Window::CreateSurface(const Window& window, void* renderApiInstance, void* outSurface)
	{
		SDL_Vulkan_CreateSurface((SDL_Window*)window.WindowInstance, *((VkInstance*)renderApiInstance), (VkSurfaceKHR*)outSurface);
	}

	void Window::Destroy(Window& window)
	{
		SDL_DestroyWindow((SDL_Window*)window.WindowInstance);
		ZeroMemory(&window, sizeof(Window));
	}

	bool Window::IsMinimized(const Window& window)
	{
		Uint32 flags = SDL_GetWindowFlags((SDL_Window*)window.WindowInstance);
		return flags & SDL_WINDOW_MINIMIZED;
	}

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

	tApplication::tApplication()
		: m_windowClosed(false)
	{
	}

	void tApplication::Init(int argc, char** argv)
	{
		check(!m_engine);

		logfinfo("Cmd line: %s\n", argv[0]);
		for (int i = 1; i < argc; ++i)
		{
			logfinfo(" %s", argv[i]);
			char* it = strstr(argv[i], "-");
			if (it)
			{
				char* endVar = strstr(it + 1, ":");
				check(endVar);
				size_t varLen = static_cast<size_t>(endVar - (it + 1));
				char varName[64];
				check(varLen < 64ul);
				strncpy_s(varName, it + 1, varLen);
				SetCVar(varName, endVar + 1);
			}
		}
		loginfo("\n");

		cCfgFile iniFile(GIniFile.Get());

		int w;
		int h;
		int x;
		int y;
		bool fullscreen;
		iniFile.GetBool("Fullscreen", fullscreen, false);
		iniFile.GetInt("WindowWidth", w, 1920);
		iniFile.GetInt("WindowHeight", h, 1080);
		iniFile.GetInt("WindowX", x, 0);
		iniFile.GetInt("WindowY", y, 0);

		for (index_t i = 0; i < iniFile.GetValueCount(); ++i)
		{
			if (CVar* var = FindCVar(iniFile.GetKey(i)))
			{
				check(!var->HasFlag(CVarFlag_SetOnlyByCmd) && "CVars with flag SetOnlyByCmd can not be changed by cfg file.");
				SetCVar(var, iniFile.GetValue(i));
				logfinfo("CVar setted from %s file: %s = %s\n", GIniFile.Get(), iniFile.GetKey(i), iniFile.GetValue(i));
			}
		}

		eWindowFlags f = fullscreen ? WindowFlags_Borderless : WindowFlags_None;
		m_window = Window::Create(w, h, x, y, "MistEngine", f);
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
		while (!m_windowClosed)
		{
			Profiling::CpuProf_Reset();
			CPU_PROFILE_SCOPE(CpuTime);
			ProcessAppEvents();
			LogicProcess(0.033f);
			if (!m_windowMinimized)
			{
				if (!m_engine->RenderProcess())
					result = EXIT_FAILURE;
			}
			GFrame++;
		}
		return result;
	}

	uint64_t tApplication::GetFrame()
	{
		return GFrame;
	}

	void tApplication::ProcessAppEvents()
	{
		auto processEventLambda = [](void* e, void* userData)
			{
				check(e && userData);
				tApplication& app = *(tApplication*)userData;
				SDL_Event& ev = *(SDL_Event*)e;
				switch (ev.type)
				{
				case SDL_QUIT:
					app.m_windowClosed = true;
					break;
				case SDL_WINDOWEVENT:
				{
					{
						uint8_t windowEvent = (*(SDL_WindowEvent*)&ev).event;
						if (windowEvent == SDL_WINDOWEVENT_CLOSE)
							app.m_windowClosed = true;
						if (windowEvent == SDL_WINDOWEVENT_MINIMIZED)
							app.m_windowMinimized = true;
					}
					break;
				default:
					app.ProcessEvent(e);
					break;
				}
				}
			};
		ProcessEvents(processEventLambda, this);
		UpdateInputState();

		if (IsKeyReleased(MIST_KEY_CODE_GRAVE))
			CVar_ShowConsole.Set(!CVar_ShowConsole.Get());
		if ((GetKeyboardState(MIST_KEY_CODE_LCTRL) || GetKeyboardState(MIST_KEY_CODE_RCTRL)) && IsKeyReleased(MIST_KEY_CODE_R))
			m_engine->ReloadShaders();
		if (IsKeyReleased(MIST_KEY_CODE_TAB))
			CVar_ShowImGui.Set(!CVar_ShowImGui.Get());
		if (IsKeyReleased(MIST_KEY_CODE_F9))
			CVar_ShowStats.Set((CVar_ShowStats.Get() + 1) % 3);
	}

	void tApplication::ProcessEvent(void* e)
	{
	}
}
