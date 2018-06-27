/*
* “龙裔自然对话”服务端主程序
*/

#include <Windows.h>
#include "DSNService.h"
#include "aixlog.hpp"
#include "utils.h"
#include <functional>

const char *DSN_LOG_FILE = "./DragonbornSpeaksNaturally.log";

int main(int argc, char* argv[])
{
	// DragonbornSpeaksNaturally.exe --encoding UTF-8
	// running from dragonborn_speaks_naturally.dll
	if (argc == 3) {
		SetCurrentDirectory(getModuleDirectory().c_str());
		SetConsoleCP(CP_UTF8);
		SetConsoleOutputCP(CP_UTF8);
	}

	AixLog::Log::init<AixLog::SinkFile>(AixLog::Severity::trace, AixLog::Type::all, DSN_LOG_FILE);

	DSNService dsnService;
	dsnService.start();
}
