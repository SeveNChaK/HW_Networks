#ifndef DECLARATION_H
#define DECLARATION_H

/**
4096 - максимальная длина пути в линукс
255 - максимальная длина файла в линукс
1 - символ конца строки
(но это все не точно)
*/
#define SIZE_MSG 5000
#define MAX_ARG_IN_CMD 100
#define MAX_ARG_SIZE 255
#define DEFAULT_SIZE_STRING 100
#define SIZE_ERR_STRING 300

#define SIZE_ADDR 20

#define MAX_LENGTH_FILE_NAME 255
#define MAX_COUNT_DIR 100

#define CODE_CMD 0
#define CODE_REQUEST_FILE 100
#define CODE_FILE_SECTION 101
#define CODE_FILE_LAST_SECTION 102
#define CODE_OK 200
#define CODE_ERROR 300
#define CODE_INFO 400
#define CODE_YOUR_PATH 401

#define CODE_TEST 6


/**
Коды пакетов:
	0 - пакет с командой. Содержит строку команды.
	1 - пакет с кусокм файла НЕ последним;
	2 - пакет с последним куском файла;
	3 - пакет с информацией об ошибке. Содержит описание ошибки;
	4 - пакет с какой-либо информацией;
	5 - пакет сообщяющий, что все хорошо;
	10 - ожидание файла на прием.
*/
struct Package {
	int code;
	char data[SIZE_MSG];
};

#endif
