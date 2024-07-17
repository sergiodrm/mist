#pragma once

namespace Mist
{
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
	};
}
