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
#define CODE_RESPONSE_FILE 101
#define CODE_FILE_SECTION 102
#define CODE_FILE_END 103
#define CODE_OK 200
#define CODE_ERROR 300
#define CODE_INFO 400
#define CODE_YOUR_PATH 401

struct Package {
	int code;
	int sizeData;
	char data[SIZE_MSG];
};

#endif
