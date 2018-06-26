/*
* “龙裔自然对话”服务端（负责语音识别）
*/
#include "SpeechRecognizer.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <process.h>

#include "api_speech_recognizer.h"

const char * SpeechRecognizer::API_LOGIN_INFO = "appid = 5b30794f";
const char * SpeechRecognizer::ASR_RES_PATH   = "fo|res/asr/common.jet";  //离线语法识别资源路径
#ifdef _WIN64
const char * SpeechRecognizer::GRM_BUILD_PATH = "res/asr/GrmBuild_x64";  //构建离线语法识别网络生成数据保存路径
#else
const char * SpeechRecognizer::GRM_BUILD_PATH = "res/asr/GrmBuild";  //构建离线语法识别网络生成数据保存路径
#endif
const char * SpeechRecognizer::GRM_FILE       = "dsn.bnf"; //构建离线识别语法网络所用的语法文件
const char * SpeechRecognizer::LEX_NAME       = "cmd"; //更新离线识别语法的cmd槽

bool SpeechRecognizer::apiIsLogin = false; // 是否已登录API（第一次调用语音识别API时需要登录）


int SpeechRecognizer::build_grm_cb(int ecode, const char *info, void *udata)
{
	SpeechRecognizer *sr = (SpeechRecognizer *)udata;

	if (NULL != sr) {
		sr->errcode = ecode;
		SetEvent(sr->eventBuildFinish);
	}

	if (MSP_SUCCESS == ecode && NULL != info) {
		printf("构建语法成功！ 语法ID:%s\n", info);
		if (NULL != sr)
			_snprintf(sr->grammar_id, MAX_GRAMMARID_LEN - 1, info);
	}
	else
		printf("构建语法失败！%d\n", ecode);

	return 0;
}

int SpeechRecognizer::updateCommandList(const std::vector<std::string>& commandList)
{
	std::string commands;

	for (size_t i = 0; i < commandList.size(); i++) {
		if (i > 0) {
			commands += '\n';
		}
		commands += formatCommandWords(commandList[i], (int)i);
	}

	return update_lexicon(commands.c_str());
}

int SpeechRecognizer::init()
{
	int ret;

	ret = api_login();
	if (MSP_SUCCESS != ret) {
		return ret;
	}

	char grm_build_params[MAX_PARAMS_LEN]    = {'\0'};

	_snprintf(grm_build_params, MAX_PARAMS_LEN - 1, 
		"engine_type = local, \
		 asr_res_path = %s, sample_rate = %d, \
		 grm_build_path = %s, ",
		ASR_RES_PATH,
		SAMPLE_RATE_16K,
		GRM_BUILD_PATH
		);

	eventBuildFinish = CreateEvent(NULL, FALSE, FALSE, NULL);

	std::string grm_content =
		"#BNF+IAT 1.0 UTF-8;\n"
		"!grammar cmd;\n"
		"!slot <cmd>;\n"
		"!start <cmd>;\n"
		"<cmd>:init!id(0);";

	ret = QISRBuildGrammar("bnf", grm_content.c_str(), (int)grm_content.size(), grm_build_params, build_grm_cb, this);

	if (MSP_SUCCESS == ret) {
		WaitForSingleObject(eventBuildFinish, INFINITE);
		ret = this->errcode;
	}

	if (eventUpdateFinish) {
		CloseHandle(eventBuildFinish);
		eventBuildFinish = NULL;
	}

	return ret;
}

void SpeechRecognizer::setResultCallback(ResultCallback callback)
{
	resultCallback = callback;
}

int SpeechRecognizer::update_lex_cb(int ecode, const char *info, void *udata)
{
	SpeechRecognizer *sr = (SpeechRecognizer *)udata;

	if (NULL != sr) {
		sr->errcode = ecode;
		SetEvent(sr->eventUpdateFinish);
	}

	if (MSP_SUCCESS == ecode)
		printf("更新词典成功！\n");
	else
		printf("更新词典失败！%d\n", ecode);

	return 0;
}

int SpeechRecognizer::update_lexicon(const char *lex_content)
{
	unsigned int lex_cnt_len                  = (unsigned int)strlen(lex_content);
	char update_lex_params[MAX_PARAMS_LEN]    = {'\0'}; 

	_snprintf(update_lex_params, MAX_PARAMS_LEN - 1, 
		"engine_type = local, text_encoding = UTF-8, \
		 asr_res_path = %s, sample_rate = %d, \
		 grm_build_path = %s, grammar_list = %s, ",
		ASR_RES_PATH,
		SAMPLE_RATE_16K,
		GRM_BUILD_PATH,
		this->grammar_id);

	eventUpdateFinish = CreateEvent(NULL, FALSE, FALSE, NULL);

	int ret = QISRUpdateLexicon(LEX_NAME, lex_content, lex_cnt_len, update_lex_params, update_lex_cb, this);

	if (MSP_SUCCESS == ret) {
		WaitForSingleObject(eventUpdateFinish, INFINITE);
		ret = this->errcode;
	}

	if (eventUpdateFinish) {
		CloseHandle(eventUpdateFinish);
		eventUpdateFinish = NULL;
	}

	return ret;
}

int SpeechRecognizer::api_login()
{
	// 已登录，不用重复登陆
	if (apiIsLogin) {
		return MSP_SUCCESS;
	}

	int ret = MSPLogin(NULL, NULL, API_LOGIN_INFO); //第一个参数为用户名，第二个参数为密码，传NULL即可，第三个参数是登录参数

	if (MSP_SUCCESS == ret) {
		apiIsLogin = true;
	}

	return ret;
}

void SpeechRecognizer::on_result(const char *result, char is_last, void *udata)
{
	SpeechRecognizer *sr = (SpeechRecognizer *)udata;

	printf("\nResult: [ %s ]\n", result);

	if (sr->resultCallback) {
		sr->resultCallback(0, 0);
	}
}
void SpeechRecognizer::on_speech_begin(void *udata)
{
	printf("Start Listening...\n");
}
void SpeechRecognizer::on_speech_end(int reason, void *udata)
{
	SpeechRecognizer *sr = (SpeechRecognizer *)udata;

	if (reason == END_REASON_VAD_DETECT)
		printf("\nSpeaking done \n");
	else
		printf("\nRecognizer error %d\n", reason);

	SetEvent(sr->events[EVT_STOP]);
}

std::string SpeechRecognizer::formatCommandWords(std::string command, int id)
{
	for (size_t i = 0; i < command.size(); i++) {
		if (command[i] == ' ' || command[i] == '\r' || command[i] == '\n' ||
			command[i] == '\t' || command[i] == '\0' || command[i] == '\x0b' ||
			command[i] == '!' || command[i] == '(' || command[i] == ')') {
			command[i] = ',';
		}
	}
	command += "!id(";
	command += std::to_string(id);
	command += ')';
	return command;
}

/* demo recognize the audio from microphone */
void SpeechRecognizer::start_recognize(const char* session_begin_params)
{
	int errcode;
	int i = 0;
	HANDLE helper_thread = NULL;

	struct speech_rec asr;
	DWORD waitres;
	char isquit = 0;

	struct speech_rec_notifier recnotifier = {
		this,
		on_result,
		on_speech_begin,
		on_speech_end
	};

	errcode = sr_init(&asr, session_begin_params, SR_MIC, DEFAULT_INPUT_DEVID, &recnotifier);
	if (errcode) {
		printf("speech recognizer init failed\n");
		return;
	}

	for (i = 0; i < EVT_TOTAL; ++i) {
		events[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
	}

	if (errcode = sr_start_listening(&asr, FALSE)) {
		printf("start listen failed %d\n", errcode);
		isquit = 1;
	}

 	while (!isquit) {
		waitres = WaitForMultipleObjects(EVT_TOTAL, events, FALSE, INFINITE);
		switch (waitres) {
		case WAIT_OBJECT_0 + EVT_STOP:
			if (errcode = sr_stop_listening(&asr)) {
				printf("stop listening failed %d\n", errcode);
				isquit = 1;
			}
			sr_uninit(&asr);

			// auto restart speech recognizer
			if (errcode = sr_init(&asr, session_begin_params, SR_MIC, DEFAULT_INPUT_DEVID, &recnotifier)) {
				printf("speech recognizer init failed\n");
				isquit = 1;
			}
			if (errcode = sr_start_listening(&asr, FALSE)) {
				printf("start listen failed %d\n", errcode);
				isquit = 1;
			}
			break;
		case WAIT_OBJECT_0 + EVT_QUIT:
			isquit = 1;
			if (errcode = sr_stop_listening(&asr)) {
				printf("stop listening failed %d\n", errcode);
			}
			break;
		default:
			printf("Why it happened !?\n");
			break;
		}
	}

	for (i = 0; i < EVT_TOTAL; ++i) {
		if (events[i]) {
			CloseHandle(events[i]);
			events[i] = NULL;
		}
	}

	sr_uninit(&asr);
}

int SpeechRecognizer::startRecognize()
{
	char asr_params[MAX_PARAMS_LEN]    = {'\0'};
	const char *rec_rslt               = NULL;
	const char *session_id             = NULL;
	const char *asr_audiof             = NULL;
	FILE *f_pcm                        = NULL;
	char *pcm_data                     = NULL;
	long pcm_count                     = 0;
	long pcm_size                      = 0;
	int last_audio                     = 0;
	int aud_stat                       = MSP_AUDIO_SAMPLE_CONTINUE;
	int ep_status                      = MSP_EP_LOOKING_FOR_SPEECH;
	int rec_status                     = MSP_REC_STATUS_INCOMPLETE;
	int rss_status                     = MSP_REC_STATUS_INCOMPLETE;
	int errcode                        = -1;
	int aud_src                        = 0;

	//离线语法识别参数设置
	_snprintf(asr_params, MAX_PARAMS_LEN - 1, 
		"engine_type = local, \
		 asr_res_path = %s, sample_rate = %d, \
		 grm_build_path = %s, local_grammar = %s, \
		 result_type = xml, result_encoding = UTF-8, \
		 vad_enable = 1, vad_bos = 10000, vad_eos = 50, \
         asr_denoise = 1, ",
		ASR_RES_PATH,
		SAMPLE_RATE_16K,
		GRM_BUILD_PATH,
		this->grammar_id
		);
	
	start_recognize(asr_params);
	return 0;
}
