/*
* ��������Ȼ�Ի�������ˣ���������ʶ��
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

	int build_grammar(); //��������ʶ���﷨����
	int update_lexicon(const char *content); //��������ʶ���﷨�ʵ�
	int run_asr(); //���������﷨ʶ��
	int is_build_fini() { return build_fini; }
	int is_update_fini() { return update_fini; }
	int status() { return errcode; }

protected:
	void start_recognize(const char* session_begin_params);

	static const int SAMPLE_RATE_16K = 16000;
	static const int SAMPLE_RATE_8K = 8000;
	static const int MAX_GRAMMARID_LEN = 32;
	static const int MAX_PARAMS_LEN = 1024;

	static const char *ASR_RES_PATH; //�����﷨ʶ����Դ·��
	static const char *GRM_BUILD_PATH; //���������﷨ʶ�������������ݱ���·��
	static const char *GRM_FILE; //��������ʶ���﷨�������õ��﷨�ļ�
	static const char *LEX_NAME; //������ʶ���﷨��cmd��

	HANDLE events[EVT_TOTAL] = { NULL,NULL,NULL };

	int     build_fini = 0;  //��ʶ�﷨�����Ƿ����
	int     update_fini = 0; //��ʶ���´ʵ��Ƿ����
	int     errcode = 0; //��¼�﷨��������´ʵ�ص�������
	char    grammar_id[MAX_GRAMMARID_LEN] = { '\0' }; //�����﷨�������ص��﷨ID

};

#endif