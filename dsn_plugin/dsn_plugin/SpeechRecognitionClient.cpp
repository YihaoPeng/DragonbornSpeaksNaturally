#include "SpeechRecognitionClient.h"
#include "ConsoleCommandRunner.h"
#include "Log.h"
#include <io.h>
#include <fcntl.h>

#define BUFSIZE 4096

HANDLE g_hChildStd_IN_Rd = NULL;
HANDLE g_hChildStd_IN_Wr = NULL;
HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;

static std::vector<std::string> split(const std::string &s, char delim) {
	std::stringstream ss(s);
	std::string item;
	std::vector<std::string> tokens;
	while (std::getline(ss, item, delim)) {
		tokens.push_back(item);
	}
	return tokens;
}

SpeechRecognitionClient* SpeechRecognitionClient::instance = NULL;

SpeechRecognitionClient* SpeechRecognitionClient::getInstance() {
	if (!instance)
		instance = new SpeechRecognitionClient();
	return instance;
}

SpeechRecognitionClient::SpeechRecognitionClient()
{
}

SpeechRecognitionClient::~SpeechRecognitionClient()
{
}

void SpeechRecognitionClient::StopDialogue() {
	WriteLine("STOP_DIALOGUE");
	this->currentDialogueList.clear();
}

void SpeechRecognitionClient::StartDialogue(DialogueList list) {
	// Same as the last dialogue, no need to start recognizing again
	if (list == this->currentDialogueList) {
		return;
	}

	this->currentDialogueList = list;
	this->currentDialogueId++;
	this->selectedIndex = -1;
	std::string command = "START_DIALOGUE|";
	command.append(std::to_string(this->currentDialogueId));
	for (size_t i = 0; i < list.lines.size(); i++) {
		command.append("|");
		command.append(list.lines[i]);
	}
	this->WriteLine(command);
}

int SpeechRecognitionClient::ReadSelectedIndex() {
	int t = this->selectedIndex;
	this->selectedIndex = -1;
	return t;
}

std::string SpeechRecognitionClient::PopCommand() {
	std::lock_guard<std::mutex> scopeLock(queueLock);
	if (queuedCommands.empty())
	{
		return "";
	}
	std::string retcommand = queuedCommands.front();
	queuedCommands.pop();
	return retcommand;
}

std::string SpeechRecognitionClient::PopEquip() {
	std::lock_guard<std::mutex> scopeLock(queueLock);
	if (queuedEquips.empty())
	{
		return "";
	}
	std::string retcommand = queuedEquips.front();
	queuedEquips.pop();
	return retcommand;
}

void SpeechRecognitionClient::EnqueueCommand(std::string command) {
	// The custom command will be executed on the current thread,
	// and the Skyrim command will be executed in the game thread.
	if (ConsoleCommandRunner::TryRunCustomCommand(command)) {
		return;
	}
	std::lock_guard<std::mutex> scopeLock(queueLock);
	queuedCommands.push(command);
}

void SpeechRecognitionClient::EnqueueEquip(std::string equip) {
	std::lock_guard<std::mutex> scopeLock(queueLock);
	queuedEquips.push(equip);
}

void SpeechRecognitionClient::AwaitResponses() {

	for (;;) {
		std::string inLine = ReadLine();
		std::vector<std::string> tokens = split(inLine, '|');
		std::string responseType = tokens[0];
		if (responseType == "DIALOGUE") {
			int dialogueId = std::stoi(tokens[1]);
			int indexId = std::stoi(tokens[2]);
			if (dialogueId == this->currentDialogueId) {
				this->selectedIndex = indexId;
			}
		}
		else if (responseType == "COMMAND") {
			std::vector<std::string> commands = split(tokens[1], ';');
			for(int i = 0; i < commands.size(); i++)
				this->EnqueueCommand(commands[i]);
		}
		else if (responseType == "EQUIP") {
			this->EnqueueEquip(tokens[1]);
		}
	}
}

std::string SpeechRecognitionClient::ReadLine() {
	DWORD dwRead = 0;
	char buffer[4096];

	std::string line(workingLine);
	for (;;) {
		BOOL bSuccess = ReadFile(this->stdOutRd, buffer, 4096, &dwRead, NULL);
		if (bSuccess && dwRead > 0)
		{
			std::string chunk = std::string(buffer, dwRead);
			line = line.append(chunk);
			for (size_t i = 0; i < chunk.length(); i++)
			{
				if (chunk[i] == '\n')
				{
					// Append any trailing line to the next readline call
					if (i < chunk.length() - 1)
						workingLine = chunk.substr(i + 1);
					else
						workingLine = "";

					// Remove \n
					line.pop_back();
					return line;
				}
			}
		}
		Sleep(200);
	}

	return "";
}

void SpeechRecognitionClient::WriteLine(std::string line) {
	DWORD dwWritten;
	line.push_back('\n');
	WriteFile(this->stdInWr, line.c_str(), line.length(), &dwWritten, NULL);
}

static DWORD WINAPI SpeechRecognitionClientThreadStart(void* ctx) {

	extern std::string g_dllPath;

	// Startup speech recognition service
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0);
	SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);
	CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0);
	SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0);

	std::string exePath = "\"" + g_dllPath.substr(0, g_dllPath.find_last_of("\\/"))
		.append("\\DragonbornSpeaksNaturally.exe\"")
		.append(" --encoding UTF-8"); // Let the service set encoding of its stdin/stdout to UTF-8.
                                    // This can avoid non-ASCII characters (such as Chinese characters) garbled.

	Log::info("\n***************************************\n");
	Log::info("Starting speech recognition service at ");
	Log::info(exePath);

	LPSTR szCmdline = const_cast<char *>(exePath.c_str());
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;
	BOOL bSuccess = FALSE;
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	//siStartInfo.hStdError = g_hChildStd_OUT_Wr;
	siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
	siStartInfo.hStdInput = g_hChildStd_IN_Rd;
	siStartInfo.wShowWindow = SW_HIDE;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
	siStartInfo.dwFlags |= STARTF_USESHOWWINDOW;

	bSuccess = CreateProcess(NULL,
		szCmdline,     // command line
		NULL,          // process security attributes
		NULL,          // primary thread security attributes
		TRUE,          // handles are inherited
		0,             // creation flags
		NULL,          // use parent's environment
		NULL,          // use parent's current directory
		&siStartInfo,  // STARTUPINFO pointer
		&piProcInfo);  // receives PROCESS_INFORMATION

	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);

	if (bSuccess)
	{
		Log::info("Initialized speech recognition service");
		SpeechRecognitionClient::getInstance()->SetHandles(g_hChildStd_IN_Wr, g_hChildStd_OUT_Rd);
		SpeechRecognitionClient::getInstance()->AwaitResponses();
	}
	else
	{
		Log::info("Failed to initialize speech recognition service");
	}

	return 0;
}


void SpeechRecognitionClient::Initialize() {
	CreateThread(NULL, 0, SpeechRecognitionClientThreadStart, NULL, 0L, NULL);
}
