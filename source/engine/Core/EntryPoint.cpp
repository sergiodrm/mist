
#include "Application/Application.h"
#include "Core/Logger.h"
#include "Core/SystemMemory.h"

int main(int argc, char* argv[])
{
	Mist::tApplication* app;
	int exitCode = 0;
	{
		PROFILE_SCOPE_LOG(InitApp, "Init app");
		Mist::InitSytemMemory();
		Mist::InitLog("log.html");
		app = Mist::tApplication::CreateApplication(argc, argv);
	}
	{
		PROFILE_SCOPE_LOG(RunApp, "Run app");
		exitCode = app->Run();
	}
	{
		PROFILE_SCOPE_LOG(InitApp, "Destroy app");
		Mist::tApplication::DestroyApplication(app);
	}
	Mist::TerminateLog();
	Mist::TerminateSystemMemory();
	return exitCode;
}
