/*
* “龙裔自然对话”服务端（负责语音识别）
*/
#include "SpeechRecognizer.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <process.h>

#include "api_speech_recognizer.h"
#include "utilities_js.hpp"

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
	int ret = 0;

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


	std::string grm_content =
		"#BNF+IAT 1.0 UTF-8;\n"
		"!grammar cmd;\n"
		"!slot <cmd>;\n"
		"!start <cmd>;\n"
		"<cmd>:init!id(0);";

	ResetEvent(eventBuildFinish);
	ret = QISRBuildGrammar("bnf", grm_content.c_str(), (int)grm_content.size(), grm_build_params, build_grm_cb, this);

	if (MSP_SUCCESS == ret) {
		WaitForSingleObject(eventBuildFinish, INFINITE);
		ret = this->errcode;
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
	// pause the recognition thread if it running
	if (isRecognizing) {
		ResetEvent(eventPauseFinish);
		SetEvent(events[EVT_PAUSE]);
		WaitForSingleObject(eventPauseFinish, INFINITE);
	}

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

	ResetEvent(eventUpdateFinish);
	int ret = QISRUpdateLexicon(LEX_NAME, lex_content, lex_cnt_len, update_lex_params, update_lex_cb, this);

	if (MSP_SUCCESS == ret) {
		WaitForSingleObject(eventUpdateFinish, INFINITE);
		ret = this->errcode;
	}

	// resume the recognition thread if it running
	if (isRecognizing) {
		SetEvent(eventCanResume);
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

	JsonNode r;
	if (!JsonNode::parse(result, result + strlen(result), r)) {
		printf("result JSON parse failed!\n");
		return;
	}

	/* Result Format:
	{
	  "sn":1, "ls":true, "bg":0, "ed":0,
	  "ws":[{
		  "bg":0,
		  "cw":[{
			  "gm":0,
			  "w":"Fus,Ro,Dah",
			  "id":4,
			  "sc":29
			}],
		  "slot":"<cmd>"
		}],
	  "sc":36
	} */
	if (r.type() != Utilities::JS::type::Obj ||
		r["ws"].type() != Utilities::JS::type::Array ||
		r["ws"].children()->size() < 1) {
		printf("result check fields failure!\n");
	}
	JsonNode ws = r["ws"].children()->at(0);
	if (ws.type() != Utilities::JS::type::Obj ||
		ws["cw"].type() != Utilities::JS::type::Array ||
		ws["cw"].children()->size() < 1) {
		printf("result check fields failure!\n");
	}
	JsonNode cw = ws["cw"].children()->at(0);
	if (cw.type() != Utilities::JS::type::Obj ||
		cw["w"].type()  != Utilities::JS::type::Str ||
		cw["id"].type() != Utilities::JS::type::Int ||
		cw["sc"].type() != Utilities::JS::type::Int) {
		printf("result check fields failure!\n");
	}

	int id = cw["id"].int32();
	int confidence = cw["sc"].int32();
	std::string words = cw["w"].str();

	printf("id: %d, confidence: %d, words: %s\n", id, confidence, words.c_str());

	if (sr->resultCallback) {
		sr->resultCallback(id, confidence);
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

	SetEvent(sr->events[EVT_RESTART]);
}

SpeechRecognizer::SpeechRecognizer()
{
	eventBuildFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	eventUpdateFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	eventStopFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	eventPauseFinish = CreateEvent(NULL, FALSE, FALSE, NULL);
	eventCanResume = CreateEvent(NULL, FALSE, FALSE, NULL);

	for (int i = 0; i < EVT_TOTAL; ++i) {
		events[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
	}
}

SpeechRecognizer::~SpeechRecognizer()
{
	CloseHandle(eventBuildFinish);
	CloseHandle(eventUpdateFinish);
	CloseHandle(eventStopFinish);
	CloseHandle(eventPauseFinish);
	CloseHandle(eventCanResume);

	for (int i = 0; i < EVT_TOTAL; ++i) {
		CloseHandle(events[i]);
	}
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
int SpeechRecognizer::start_recognize(const char* session_begin_params)
{
	int errcode;
	int i = 0;

	struct speech_rec asr;
	DWORD waitres;
	char isquit = 0;

	struct speech_rec_notifier recnotifier = {
		this,
		on_result,
		on_speech_begin,
		on_speech_end
	};

	isRecognizing = true;

	errcode = sr_init(&asr, session_begin_params, SR_MIC, DEFAULT_INPUT_DEVID, &recnotifier);
	if (errcode) {
		printf("speech recognizer init failed\n");
		SetEvent(eventStopFinish);

		isRecognizing = false;
		return errcode;
	}


	if (errcode = sr_start_listening(&asr, FALSE)) {
		printf("start listen failed %d\n", errcode);
		isquit = 1;
	}

 	while (!isquit) {
		waitres = WaitForMultipleObjects(EVT_TOTAL, events, FALSE, INFINITE);
		switch (waitres) {
		case WAIT_OBJECT_0 + EVT_RESTART:
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
		case WAIT_OBJECT_0 + EVT_PAUSE:
			if (errcode = sr_stop_listening(&asr)) {
				printf("stop listening failed %d\n", errcode);
				isquit = 1;
			}
			sr_uninit(&asr);

			// notify the caller that I have stopped
			ResetEvent(eventCanResume);
			SetEvent(eventPauseFinish);

			// wait for the caller to allow me to restart
			WaitForSingleObject(eventCanResume, INFINITE);

			// restart speech recognizer
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

	sr_uninit(&asr);

	SetEvent(eventStopFinish);

	isRecognizing = false;
	return MSP_SUCCESS;
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
		 result_type = json, result_encoding = UTF-8, \
		 vad_enable = 1, vad_bos = 10000, vad_eos = 50, \
         asr_denoise = 0, ",
		ASR_RES_PATH,
		SAMPLE_RATE_16K,
		GRM_BUILD_PATH,
		this->grammar_id
		);
	
	return start_recognize(asr_params);
}

void SpeechRecognizer::stopRecognize()
{
	// send signal for recognition thread
	ResetEvent(eventStopFinish);
	SetEvent(events[EVT_QUIT]);
	// wait recognition thread's quit signal
	WaitForSingleObject(eventStopFinish, INFINITE);
}
