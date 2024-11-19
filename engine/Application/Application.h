#pragma once
#include <cstdint>

namespace Mist
{
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
		static Window Create(uint32_t width, uint32_t height, uint32_t posx, uint32_t posy, const char* title);
		static void CreateSurface(const Window& window, void* renderApiInstance, void* outSurface);
		static void Destroy(Window& window);
	};

	class tApplication
	{
	public:

		static tApplication* CreateApplication(int argc, char** argv);
		static void DestroyApplication(tApplication* app);

		virtual ~tApplication() = default;
		virtual void Init(int argc, char** argv);
		virtual void Destroy();

		int Run();
		class IRenderEngine* GetEngineInstance() const { return m_engine; }
	protected:
		virtual void LogicProcess(float deltaTime) {}

	private:
		class IRenderEngine* m_engine = nullptr;
		Window m_window;
	};
}
