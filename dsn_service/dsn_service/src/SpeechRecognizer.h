/*
* “龙裔自然对话”服务端（负责语音识别）
*/
#ifndef __SPEECH_RECOGNIZER__
#define __SPEECH_RECOGNIZER__

// disable "This function or variable may be unsafe"
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <string>
#include <map>
#include <vector>
#include <functional>

#include "../../../include/qisr.h"
#include "../../../include/msp_cmn.h"
#include "../../../include/msp_errors.h"

class SpeechRecognizer {
public:
	enum {
		EVT_START = 0,
		EVT_STOP,
		EVT_QUIT,
		EVT_TOTAL
	};

	using ResultCallback = std::function<void (int /*id*/, int /*confidence*/)>;

	// callback functions of result
	static int build_grm_cb(int ecode, const char *info, void *udata);
	static int update_lex_cb(int ecode, const char *info, void *udata);

	// callback functions of event
	static void on_result(const char *result, char is_last, void *udata);
	static void on_speech_begin(void *udata);
	static void on_speech_end(int reason, void *udata);

	// normal public functions
	int init(); //构建离线识别语法网络
	void setResultCallback(ResultCallback callback);
	int updateCommandList(const std::vector<std::string> &commandList); // 更新待识别的命令词
	int startRecognize(); //进行离线语法识别

protected:
	static std::string formatCommandWords(std::string command, int id);

	void start_recognize(const char* session_begin_params);
	int update_lexicon(const char *content); //更新离线识别语法词典

	int  api_login();

	static const int SAMPLE_RATE_16K = 16000;
	static const int SAMPLE_RATE_8K = 8000;
	static const int MAX_GRAMMARID_LEN = 32;
	static const int MAX_PARAMS_LEN = 1024;

	static const char *API_LOGIN_INFO; //讯飞 API 登录/授权时使用的信息
	static const char *ASR_RES_PATH; //离线语法识别资源路径
	static const char *GRM_BUILD_PATH; //构建离线语法识别网络生成数据保存路径
	static const char *GRM_FILE; //构建离线识别语法网络所用的语法文件
	static const char *LEX_NAME; //新离线识别语法的cmd槽

	static bool apiIsLogin; // 是否已登录API（第一次调用语音识别API时需要登录）

	HANDLE events[EVT_TOTAL] = { NULL,NULL,NULL };
	HANDLE eventBuildFinish = NULL; //语法构建完成事件
	HANDLE eventUpdateFinish = NULL; //更新词典完成事件

	volatile int errcode = 0; //记录语法构建或更新词典回调错误码
	char grammar_id[MAX_GRAMMARID_LEN] = { '\0' }; //保存语法构建返回的语法ID

	ResultCallback resultCallback = nullptr;
};

#endif