
#include "Application/Application.h"
#include "Core/Logger.h"
#include "Core/SystemMemory.h"

int main(int argc, char* argv[])
{
	Mist::InitSytemMemory();
	Mist::InitLog("log.html");
	Mist::tApplication* app = Mist::tApplication::CreateApplication(argc, argv);
	int exitCode = app->Run();
	Mist::tApplication::DestroyApplication(app);
	Mist::TerminateLog();
	Mist::TerminateSystemMemory();
	return exitCode;
}
