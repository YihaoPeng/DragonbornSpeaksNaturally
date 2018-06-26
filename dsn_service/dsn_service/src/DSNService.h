/*
* “龙裔自然对话”服务端（负责语音识别）
*/
#ifndef __DSN_SERVICE__
#define __DSN_SERVICE__

// disable "This function or variable may be unsafe"
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <string>

#include "../../../include/qisr.h"
#include "../../../include/msp_cmn.h"
#include "../../../include/msp_errors.h"

class DSNService {
public:
	enum {
		EVT_START = 0,
		EVT_STOP,
		EVT_QUIT,
		EVT_TOTAL
	};

	static int build_grm_cb(int ecode, const char *info, void *udata);
	static int update_lex_cb(int ecode, const char *info, void *udata);

	static void on_result(const char *result, char is_last);
	static void on_speech_begin(void *udata);
	static void on_speech_end(int reason, void *udata);

	int build_grammar(); //构建离线识别语法网络
	int update_lexicon(const char *content); //更新离线识别语法词典
	int run_asr(); //进行离线语法识别
	int is_build_fini() { return build_fini; }
	int is_update_fini() { return update_fini; }
	int status() { return errcode; }

protected:
	void start_recognize(const char* session_begin_params);

	static const int SAMPLE_RATE_16K = 16000;
	static const int SAMPLE_RATE_8K = 8000;
	static const int MAX_GRAMMARID_LEN = 32;
	static const int MAX_PARAMS_LEN = 1024;

	static const char *ASR_RES_PATH; //离线语法识别资源路径
	static const char *GRM_BUILD_PATH; //构建离线语法识别网络生成数据保存路径
	static const char *GRM_FILE; //构建离线识别语法网络所用的语法文件
	static const char *LEX_NAME; //新离线识别语法的cmd槽

	HANDLE events[EVT_TOTAL] = { NULL,NULL,NULL };

	int     build_fini = 0;  //标识语法构建是否完成
	int     update_fini = 0; //标识更新词典是否完成
	int     errcode = 0; //记录语法构建或更新词典回调错误码
	char    grammar_id[MAX_GRAMMARID_LEN] = { '\0' }; //保存语法构建返回的语法ID

};

#endif