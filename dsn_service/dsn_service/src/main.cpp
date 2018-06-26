/*
* “龙裔自然对话”服务端主程序
*/

#include <conio.h>
#include "SpeechRecognizer.h"

void result_callback(int id, int confidence) {
	printf("result_callback, id: %d, confidence: %d\n", id, confidence);
}

int main(int argc, char* argv[])
{
	SpeechRecognizer SpeechRecognizer;
	int ret = 0;

	SpeechRecognizer.setResultCallback(result_callback);

	printf("构建离线识别语法网络...\n");
	ret = SpeechRecognizer.init();  //第一次使用某语法进行识别，需要先构建语法网络，获取语法ID，之后使用此语法进行识别，无需再次构建
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
		"Fus Ro Dah",
		"装备血吟刃",
		"装备黑檀弓",
		"我需要治疗",
		"把那龙打下来",
		"做好战斗准备"
	});
	if (MSP_SUCCESS != ret) {
		printf("更新词典调用失败！\n");
		goto exit;
	}
	printf("更新离线语法词典完成，开始识别...\n");

	ret = SpeechRecognizer.startRecognize();
	if (MSP_SUCCESS != ret) {
		printf("离线语法识别出错: %d \n", ret);
		goto exit;
	}

exit:
	printf("\n请按任意键退出...\n");

	while (_getch() != '\r');
	return 0;
}
