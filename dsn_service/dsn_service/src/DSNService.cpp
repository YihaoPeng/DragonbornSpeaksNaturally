/*
* “龙裔自然对话”服务端（负责语音识别）
*/
#include "DSNService.h"
#include "utils.h"

#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <map>

const wchar_t * DSNService::CONFIG_INI_PATH = L"./DragonbornSpeaksNaturally.ini"; // 配置文件路径

void DSNService::result_callback(int id, int confidence) {
	printf("result_callback, id: %d, confidence: %d\n", id, confidence);

	if (isRecognizingDialog && id < dialogList.size() && confidence >= dialogueMinConfidence) {
		std::cout << dialogList[id] << std::endl;
	}
	else if (id < commandList.size() && confidence >= commandMinConfidence) {
		std::cout << commandList[id] << std::endl;
	}
}

void DSNService::readConfigureFromIniFile()
{
	////////////////////////// read confidences //////////////////////////

	dialogueMinConfidence = GetPrivateProfileInt(L"SpeechRecognition", L"dialogueMinConfidenceXunfei", 0, CONFIG_INI_PATH);
	commandMinConfidence = GetPrivateProfileInt(L"SpeechRecognition", L"commandMinConfidenceXunfei", 0, CONFIG_INI_PATH);


	////////////////////////// read console commands //////////////////////////

	commandList.clear();
	commandPhraseList.clear();

	std::wstring commandBuf;
	commandBuf.resize(102400); // 100KB

	size_t copiedChars = GetPrivateProfileSection(L"ConsoleCommands", (LPWSTR)commandBuf.data(), (DWORD)commandBuf.size(), CONFIG_INI_PATH);
	if (copiedChars == 0) {
		return;
	}
	commandBuf.resize(copiedChars);

	size_t i = 0;
	for (size_t j = 0; j < commandBuf.size(); j++) {
		if (commandBuf[j] == L'\0' && j > i) {
			std::string commandLine = UnicodeToUTF8(commandBuf.substr(i, j - i));

			size_t pos = commandLine.find('=');
			if (pos != commandLine.npos) {
				std::string commandPhrase = commandLine.substr(0, pos);
				std::string command = commandLine.substr(pos + 1);

				printf("command: %s = %s\n", commandPhrase.c_str(), command.c_str());

				commandPhraseList.push_back(commandPhrase);
				commandList.push_back("COMMAND|" + command);
			}

			i = j + 1;
		}
	}

	
}

std::vector<std::string> DSNService::string_split(const std::string &s, char delim) {
	std::stringstream ss(s);
	std::string item;
	std::vector<std::string> tokens;
	while (std::getline(ss, item, delim)) {
		tokens.push_back(item);
	}
	return tokens;
}

DWORD DSNService::readlineThread(void * udata)
{
	DSNService *dsnService = (DSNService *)udata;

	std::string line;

	while (!std::cin.eof()) {
		std::getline(std::cin, line);
		if (line.empty()) {
			continue;
		}
		
		auto params = string_split(line, '|');
		int ret = dsnService->speechRecognizer.updateCommandList(params);
		if (MSP_SUCCESS != ret) {
			printf("更新词典调用失败！\n");
		}
	}

	dsnService->speechRecognizer.stopRecognize();

	return 0;
}

void DSNService::start()
{
	int ret = 0;

	readConfigureFromIniFile();
	speechRecognizer.setResultCallback(std::bind(&DSNService::result_callback, this, std::placeholders::_1, std::placeholders::_2));

	printf("构建离线识别语法网络...\n");
	ret = speechRecognizer.init();  //第一次使用某语法进行识别，需要先构建语法网络，获取语法ID，之后使用此语法进行识别，无需再次构建
	if (MSP_SUCCESS != ret) {
		printf("构建语法调用失败！\n");
		return;
	}
	printf("离线识别语法网络构建完成\n");

	printf("更新离线语法词典...\n");
	//当语法词典槽中的词条需要更新时，调用QISRUpdateLexicon接口完成更新
	ret = speechRecognizer.updateCommandList(commandPhraseList);
	if (MSP_SUCCESS != ret) {
		printf("更新词典调用失败！\n");
		return;
	}
	printf("更新离线语法词典完成，开始识别...\n");


	CreateThread(NULL, 0, DSNService::readlineThread, this, 0L, NULL);

	ret = speechRecognizer.startRecognize();
	if (MSP_SUCCESS != ret) {
		printf("离线语法识别出错: %d \n", ret);
		return;
	}
}
