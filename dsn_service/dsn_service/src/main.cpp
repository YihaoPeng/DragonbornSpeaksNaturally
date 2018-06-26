/*
* ��������Ȼ�Ի��������������
*/

#include <conio.h>
#include "DSNService.h"

/* main thread: start/stop record ; query the result of recgonization.
* record thread: record callback(data write)
* helper thread: ui(keystroke detection)
*/
int main(int argc, char* argv[])
{
	const char *login_config = "appid = 5b30794f"; //��¼����
	DSNService dsnService;
	int ret = 0;

	ret = MSPLogin(NULL, NULL, login_config); //��һ������Ϊ�û������ڶ�������Ϊ���룬��NULL���ɣ������������ǵ�¼����
	if (MSP_SUCCESS != ret) {
		printf("��¼ʧ�ܣ�%d\n", ret);
		goto exit;
	}

	printf("��������ʶ���﷨����...\n");
	ret = dsnService.build_grammar();  //��һ��ʹ��ĳ�﷨����ʶ����Ҫ�ȹ����﷨���磬��ȡ�﷨ID��֮��ʹ�ô��﷨����ʶ�������ٴι���
	if (MSP_SUCCESS != ret) {
		printf("�����﷨����ʧ�ܣ�\n");
		goto exit;
	}
	while (1 != dsnService.is_build_fini())
		Sleep(100);
	if (MSP_SUCCESS != dsnService.status())
		goto exit;
	printf("����ʶ���﷨���繹�����\n");

	printf("���������﷨�ʵ�...\n");

	//���﷨�ʵ���еĴ�����Ҫ����ʱ������QISRUpdateLexicon�ӿ���ɸ���
	ret = dsnService.update_lexicon("time,is,money!id(1)\n\
time,goes,by!id(2)\nhello,world!id(3)\n\
this,is,dragonborn!id(4)\nFus,Ro,Dah!id(5)");
	if (MSP_SUCCESS != ret) {
		printf("���´ʵ����ʧ�ܣ�\n");
		goto exit;
	}
	while (1 != dsnService.is_update_fini())
		Sleep(100);
	if (MSP_SUCCESS != dsnService.status())
		goto exit;
	printf("���������﷨�ʵ���ɣ���ʼʶ��...\n");

	ret = dsnService.run_asr();
	if (MSP_SUCCESS != ret) {
		printf("�����﷨ʶ�����: %d \n", ret);
		goto exit;
	}

exit:
	MSPLogout();
	printf("\n�밴������˳�...\n");

	while (_getch() != '\r');
	return 0;
}
