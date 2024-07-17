
#include "Application.h"
#include "Logger.h"

int main(int argc, char* argv[])
{
	Mist::InitLog("log.html");
	Mist::tApplication* app = Mist::tApplication::CreateApplication(argc, argv);
	int exitCode = app->Run();
	Mist::tApplication::DestroyApplication(app);
	Mist::TerminateLog();
	return exitCode;
}