/*
* “龙裔自然对话”服务端主程序
*/

#include <conio.h>
#include "DSNService.h"
#include "aixlog.hpp"
#include "functional"

const char *DSN_LOG_FILE = "./DragonbornSpeaksNaturally.log";

int main(int argc, char* argv[])
{
	AixLog::Log::init<AixLog::SinkFile>(AixLog::Severity::trace, AixLog::Type::all, DSN_LOG_FILE);

	DSNService dsnService;
	dsnService.start();
}
