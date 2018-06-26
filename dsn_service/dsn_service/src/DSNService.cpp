/*
* “龙裔自然对话”服务端（负责语音识别）
*/
#include "DSNService.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <process.h>

#include "speech_recognizer.h"


const char * DSNService::ASR_RES_PATH   = "fo|res/asr/common.jet";  //离线语法识别资源路径
#ifdef _WIN64
const char * DSNService::GRM_BUILD_PATH = "res/asr/GrmBuild_x64";  //构建离线语法识别网络生成数据保存路径
#else
const char * DSNService::GRM_BUILD_PATH = "res/asr/GrmBuild";  //构建离线语法识别网络生成数据保存路径
#endif
const char * DSNService::GRM_FILE       = "dsn.bnf"; //构建离线识别语法网络所用的语法文件
const char * DSNService::LEX_NAME       = "cmd"; //更新离线识别语法的cmd槽


int DSNService::build_grm_cb(int ecode, const char *info, void *udata)
{
	DSNService *dsnService = (DSNService *)udata;

	if (NULL != dsnService) {
		dsnService->build_fini = 1;
		dsnService->errcode = ecode;
	}

	if (MSP_SUCCESS == ecode && NULL != info) {
		printf("构建语法成功！ 语法ID:%s\n", info);
		if (NULL != dsnService)
			_snprintf(dsnService->grammar_id, MAX_GRAMMARID_LEN - 1, info);
	}
	else
		printf("构建语法失败！%d\n", ecode);

	return 0;
}

int DSNService::build_grammar()
{
	FILE *grm_file                           = NULL;
	char *grm_content                        = NULL;
	unsigned int grm_cnt_len                 = 0;
	char grm_build_params[MAX_PARAMS_LEN]    = {'\0'};
	int ret                                  = 0;

	grm_file = fopen(GRM_FILE, "rb");	
	if(NULL == grm_file) {
		printf("打开\"%s\"文件失败！[%s]\n", GRM_FILE, strerror(errno));
		return -1; 
	}

	fseek(grm_file, 0, SEEK_END);
	grm_cnt_len = ftell(grm_file);
	fseek(grm_file, 0, SEEK_SET);

	grm_content = (char *)malloc(grm_cnt_len + 1);
	if (NULL == grm_content)
	{
		printf("内存分配失败!\n");
		fclose(grm_file);
		grm_file = NULL;
		return -1;
	}
	fread((void*)grm_content, 1, grm_cnt_len, grm_file);
	grm_content[grm_cnt_len] = '\0';
	fclose(grm_file);
	grm_file = NULL;

	_snprintf(grm_build_params, MAX_PARAMS_LEN - 1, 
		"engine_type = local, \
		asr_res_path = %s, sample_rate = %d, \
		grm_build_path = %s, ",
		ASR_RES_PATH,
		SAMPLE_RATE_16K,
		GRM_BUILD_PATH
		);
	ret = QISRBuildGrammar("bnf", grm_content, grm_cnt_len, grm_build_params, build_grm_cb, this);

	free(grm_content);
	grm_content = NULL;

	return ret;
}

int DSNService::update_lex_cb(int ecode, const char *info, void *udata)
{
	DSNService *dsnService = (DSNService *)udata;

	if (NULL != dsnService) {
		dsnService->update_fini = 1;
		dsnService->errcode = ecode;
	}

	if (MSP_SUCCESS == ecode)
		printf("更新词典成功！\n");
	else
		printf("更新词典失败！%d\n", ecode);

	return 0;
}

int DSNService::update_lexicon(const char *lex_content)
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
	return QISRUpdateLexicon(LEX_NAME, lex_content, lex_cnt_len, update_lex_params, update_lex_cb, this);
}

void DSNService::on_result(const char *result, char is_last)
{
	printf("\nResult: [ %s ]\n", result);
}
void DSNService::on_speech_begin(void *udata)
{
	printf("Start Listening...\n");
}
void DSNService::on_speech_end(int reason, void *udata)
{
	DSNService *dsnService = (DSNService *)udata;

	if (reason == END_REASON_VAD_DETECT)
		printf("\nSpeaking done \n");
	else
		printf("\nRecognizer error %d\n", reason);

	SetEvent(dsnService->events[EVT_STOP]);
}

/* demo recognize the audio from microphone */
void DSNService::start_recognize(const char* session_begin_params)
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
		case WAIT_FAILED:
		case WAIT_TIMEOUT:
			printf("Why it happened !?\n");
			break;
		case WAIT_OBJECT_0 + EVT_STOP:		
			if (errcode = sr_stop_listening(&asr)) {
				printf("stop listening failed %d\n", errcode);
				isquit = 1;
			}
			sr_uninit(&asr);
			if (errcode = sr_init(&asr, session_begin_params, SR_MIC, DEFAULT_INPUT_DEVID, &recnotifier)) {
				printf("speech recognizer init failed\n");
				isquit = 1;
			}
			if (errcode = sr_start_listening(&asr, FALSE)) {
				printf("start listen failed %d\n", errcode);
				isquit = 1;
			}
			break;
		default:
			break;
		}
	}

	for (i = 0; i < EVT_TOTAL; ++i) {
		if (events[i])
			CloseHandle(events[i]);
	}

	sr_uninit(&asr);
}

int DSNService::run_asr()
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
