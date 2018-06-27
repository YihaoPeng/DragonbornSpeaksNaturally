/*
* “龙裔自然对话”服务端（负责语音识别）
*/
#ifndef __DSN_SERVICE_H__
#define __DSN_SERVICE_H__

// disable "This function or variable may be unsafe"
#define _CRT_SECURE_NO_WARNINGS

#include <vector>
#include <string>
#include "SpeechRecognizer.h"

class DSNService {
public:
	static DWORD WINAPI readlineThread(void* udata);

	void start();

protected:
	static std::vector<std::string> string_split(const std::string &s, char delim);

	void result_callback(int id, int confidence);
	void readCommandsFromIniFile();

	static const wchar_t * CONFIG_INI_PATH; // 配置文件路径

	SpeechRecognizer speechRecognizer;

	// command and its phrase have the same index
	std::vector<std::string> commandList;
	std::vector<std::string> commandPhraseList;

	// dialog and its phrase have the same index
	std::vector<std::string> dialogList;
	std::vector<std::string> dialogPhraseList;

	// if false, is recognizing commands
	std::atomic<bool> isRecognizingDialog = false;
};

#endif