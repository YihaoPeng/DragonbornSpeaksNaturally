/*
* “龙裔自然对话”服务端主程序
*/

#include <conio.h>
#include "SpeechRecognizer.h"

/* main thread: start/stop record ; query the result of recgonization.
* record thread: record callback(data write)
* helper thread: ui(keystroke detection)
*/
int main(int argc, char* argv[])
{
	const char *login_config = "appid = 5b30794f"; //登录参数
	SpeechRecognizer SpeechRecognizer;
	int ret = 0;

	ret = MSPLogin(NULL, NULL, login_config); //第一个参数为用户名，第二个参数为密码，传NULL即可，第三个参数是登录参数
	if (MSP_SUCCESS != ret) {
		printf("登录失败：%d\n", ret);
		goto exit;
	}

	printf("构建离线识别语法网络...\n");
	ret = SpeechRecognizer.build_grammar();  //第一次使用某语法进行识别，需要先构建语法网络，获取语法ID，之后使用此语法进行识别，无需再次构建
	if (MSP_SUCCESS != ret) {
		printf("构建语法调用失败！\n");
		goto exit;
	}
	printf("离线识别语法网络构建完成\n");

	printf("更新离线语法词典...\n");

	//当语法词典槽中的词条需要更新时，调用QISRUpdateLexicon接口完成更新
	ret = SpeechRecognizer.updateCommandList({
		"time is money",
		"time goes by",
		"hello world",
		"this is dragonborn",
		"Fus Ro Dah"
	});
	if (MSP_SUCCESS != ret) {
		printf("更新词典调用失败！\n");
		goto exit;
	}
	printf("更新离线语法词典完成，开始识别...\n");

	ret = SpeechRecognizer.run_asr();
	if (MSP_SUCCESS != ret) {
		printf("离线语法识别出错: %d \n", ret);
		goto exit;
	}

exit:
	MSPLogout();
	printf("\n请按任意键退出...\n");

	while (_getch() != '\r');
	return 0;
}
