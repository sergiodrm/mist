#pragma once
#include <cstdint>

namespace Mist
{
	enum eWindowFlagBits
	{
		WindowFlags_None = 0,
		WindowFlags_Fullscreen= 0x00000001,         /**< fullscreen window */
		WindowFlags_Shown= 0x00000002,              /**< window is visible */
		WindowFlags_Hidden= 0x00000004,             /**< window is not visible */
		WindowFlags_Borderless = 0x00000008,         /**< no window decoration */
		WindowFlags_Resizable = 0x00000010,          /**< window can be resized */
	};
	typedef uint32_t eWindowFlags;
	struct Window
	{
		void* WindowInstance;
		uint32_t Width;
		uint32_t Height;
		char Title[32];

		Window() : WindowInstance(nullptr), Width(1920), Height(1080)
		{
			*Title = 0;
		}
		static Window Create(uint32_t width, uint32_t height, uint32_t posx, uint32_t posy, const char* title, eWindowFlags flags = WindowFlags_None);
		static void CreateSurface(const Window& window, void* renderApiInstance, void* outSurface);
		static void Destroy(Window& window);

		static bool IsMinimized(const Window& window);
	};

	class tApplication
	{
	public:

		static tApplication* CreateApplication(int argc, char** argv);
		static void DestroyApplication(tApplication* app);

		tApplication();
		virtual ~tApplication() = default;
		virtual void Init(int argc, char** argv);
		virtual void Destroy();

		int Run();
		class IRenderEngine* GetEngineInstance() const { return m_engine; }
	protected:
		virtual void LogicProcess(float deltaTime) {}
		void ProcessAppEvents();
		virtual void ProcessEvent(void* e);

	private:
		class IRenderEngine* m_engine = nullptr;
		Window m_window;
		uint8_t m_windowClosed : 1;
		uint8_t m_windowMinimized : 1;
	};
}
