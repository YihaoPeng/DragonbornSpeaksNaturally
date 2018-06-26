/*
* “龙裔自然对话”服务端（负责语音识别）
*/
#ifndef __DSN_SERVICE__
#define __DSN_SERVICE__

// disable "This function or variable may be unsafe"
#define _CRT_SECURE_NO_WARNINGS

#include "SpeechRecognizer.h"

class DSNService {
public:
	void start();
protected:
	void result_callback(int id, int confidence);

	SpeechRecognizer speechRecognizer;
};

#endif