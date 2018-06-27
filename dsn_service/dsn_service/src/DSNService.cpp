/*
* “龙裔自然对话”服务端（负责语音识别）
*/
#include "DSNService.h"
#include "utils.h"
#include "aixlog.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <map>

const char * DSNService::CONFIG_INI_PATH = "./DragonbornSpeaksNaturally.ini"; // 配置文件路径

void DSNService::result_callback(int id, int confidence) {
	//LOG(INFO) << "result_callback, id: " << id << ", confidence: " << confidence << std::endl;

	if (isRecognizingDialog && id < dialogList.size() && confidence >= dialogueMinConfidence) {
		std::cout << dialogList[id] << std::endl;
		LOG(INFO) << "send: " << dialogList[id] << std::endl;
	}
	else if (id < commandList.size() && confidence >= commandMinConfidence) {
		std::cout << commandList[id] << std::endl;
		LOG(INFO) << "send: " << commandList[id] << std::endl;
	}
}

void DSNService::readConfigureFromIniFile()
{
	////////////////////////// read confidences //////////////////////////

	dialogueMinConfidence = GetPrivateProfileInt("SpeechRecognition", "dialogueMinConfidenceXunfei", 0, CONFIG_INI_PATH);
	commandMinConfidence = GetPrivateProfileInt("SpeechRecognition", "commandMinConfidenceXunfei", 0, CONFIG_INI_PATH);

	LOG(INFO) << "Min confidences, dialog: " << dialogueMinConfidence  << ", command: " << commandMinConfidence << std::endl;


	////////////////////////// read console commands //////////////////////////

	commandList.clear();
	commandPhraseList.clear();

	std::string commandBuf;
	commandBuf.resize(102400); // 100KB

	size_t copiedChars = GetPrivateProfileSection("ConsoleCommands", (LPSTR)commandBuf.data(), (DWORD)commandBuf.size(), CONFIG_INI_PATH);
	if (copiedChars == 0) {
		return;
	}
	commandBuf.resize(copiedChars);

	size_t i = 0;
	for (size_t j = 0; j < commandBuf.size(); j++) {
		if (commandBuf[j] == L'\0' && j > i) {
			std::string commandLine = commandBuf.substr(i, j - i);

			size_t pos = commandLine.find('=');
			if (pos != commandLine.npos) {
				std::string commandPhrase = commandLine.substr(0, pos);
				std::string command = commandLine.substr(pos + 1);

				LOG(INFO) << "command: " << commandPhrase.c_str() << " = " << command.c_str() << std::endl;

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
	LOG(INFO) << "readline thread running..." << std::endl;

	DSNService *dsnService = (DSNService *)udata;

	std::string line;

	while (!std::cin.eof()) {
		std::getline(std::cin, line);
		if (line.empty()) {
			continue;
		}

		LOG(INFO) << "recv: " << line << std::endl;
		
		auto params = string_split(line, '|');
		dsnService->parseReceivedCommand(params);
	}

	dsnService->speechRecognizer.stopRecognize();

	return 0;
}

void DSNService::parseReceivedCommand(const std::vector<std::string> &params) {
	if (params.empty()) {
		return;
	}

	const std::string &action = params[0];
	
	if (action == "START_DIALOGUE") {
		// recv: START_DIALOGUE|id|words1|words2|...
		// exam: START_DIALOGUE|4|hello world|time is money
		if (params.size() < 3) {
			// non words
			return;
		}

		// reply: DIALOGUE|id|wordIndex
		// examp: DIALOGUE|4|0
		//        DIALOGUE|4|1
		std::string cmdPrefix = "DIALOGUE|" + params[1] + "|";

		dialogPhraseList.clear();
		dialogList.clear();

		for (size_t i=2, j=0; i<params.size(); i++, j++) {
			dialogPhraseList.push_back(params[i]);
			dialogList.push_back(cmdPrefix + std::to_string(j));
		}

		speechRecognizer.updateCommandList(dialogPhraseList);
		isRecognizingDialog = true;
	}
	else if (action == "STOP_DIALOGUE") {
		if (isRecognizingDialog) {
			// back to recognizing commands
			speechRecognizer.updateCommandList(commandPhraseList);
			isRecognizingDialog = false;
		}
	}

}

void DSNService::start()
{
	int ret = 0;

	readConfigureFromIniFile();
	speechRecognizer.setResultCallback(std::bind(&DSNService::result_callback, this, std::placeholders::_1, std::placeholders::_2));

	LOG(INFO) << "building recognize grammar..." << std::endl;
	ret = speechRecognizer.init();  //第一次使用某语法进行识别，需要先构建语法网络，获取语法ID，之后使用此语法进行识别，无需再次构建
	if (MSP_SUCCESS != ret) {
		LOG(ERROR) << "building recognize grammar failed, errcode: " << ret << std::endl;
		return;
	}

	LOG(INFO) << "building grammar success, update wordlist..." << std::endl;
	ret = speechRecognizer.updateCommandList(commandPhraseList);
	if (MSP_SUCCESS != ret) {
		LOG(ERROR) << "update wordlist failed, errcode: " << ret << std::endl;
		return;
	}

	LOG(INFO) << "update wordlist success, start recognize..." << std::endl;

	CreateThread(NULL, 0, DSNService::readlineThread, this, 0L, NULL);

	ret = speechRecognizer.startRecognize();
	if (MSP_SUCCESS != ret) {
		LOG(ERROR) << "start recognize failed, errcode: " << ret << std::endl;
		return;
	}
}
