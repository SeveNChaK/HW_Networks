#ifndef DECLARATION_H
#define DECLARATION_H

/**
508 байт данных в UDP/IPv4 гарантирует доставку пакета без промежуточной
фрагментации и принятие любым устройством
*/
#define SIZE_PACK 500
#define SIZE_PACK_DATA 480
#define SIZE_CMD 5000 //в linux длина пути и имени файла ограничена, тут даже с запасом хватит
#define SIZE_ARG 255
#define SIZE_MY_STR 500
#define MAX_QUANTITY_ARGS_CMD 100

#define CODE_CMD 101
#define CODE_REQUEST_FILE 200
#define CODE_RESPONSE_FILE 201
#define CODE_INFO 400
#define CODE_ERROR 401
#define CODE_YOUR_PATH 500
#define CODE_OK 600
#define NO_ACK 0
#define ACK 777

struct Package {
	int ack;
	int id;
	int maxId;
	int code;
	int lengthData;
	char data[SIZE_PACK_DATA];
};

#endif
