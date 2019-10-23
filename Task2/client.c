#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <regex.h>

/*
ВОПРОСЫ:
	что если сделать const char *str на входе функции?
	конкурентный подход обращения к файлам? (копии создавать)
	отключение во время передачи файла?
	ошибки во время передачи файла?
	обращение в файлам по пути или только по имени в текущей директроии?
*/

#include "declaration.h"
#include "dexchange.h"

void execCommand(int sock);
void sendFile(int sock, char *fileName);
void readFile(int sock, char *fileName);

int main(int argc, char** argv) {

	if(argc != 3){
		fprintf(stderr, "%s\n%s\n", "Неверное количество аргументов!", "Необходим вызов: ./client [IP] [PORT]");
		exit(1);
	}
	char *ip = (char *) argv[1];
	int port = *((int*) argv[2]);

	struct sockaddr_in peer;
	peer.sin_family = AF_INET;
	peer.sin_addr.s_addr = inet_addr(ip); 
	peer.sin_port = htons(port);

	int sock = -1, rc = -1;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		
		exit(1);
	}
	rc = connect(sock, (struct sockaddr*) &peer, sizeof(peer));
	if(rc == -1){
		fprintf(stderr, "Проблемы с подключением по IP - %s и PORT - %d\n", ip, port);
		exit(1);
	}
	
	fprintf(stdout, "%s", "~$:"); fflush(stdout);

	char inputBuf[SIZE_MSG];
	for(;;){
		bzero(inputBuf, sizeof(inputBuf));
		fgets(inputBuf, sizeof(inputBuf), stdin);
		inputBuf[strlen(inputBuf) - 1] = '\0';

		if(!strcmp("quit", inputBuf)){
			close(sock);
			break;
		}

		// if(sendPack(sock, CODE_CMD, inputBuf) == -1){
		// 	fprintf(stderr, "Проблемы с отправкой команды на сервер. Соединение разорвано!\n");
		// 	break;
		// }

		if(sendPack(sock, CODE_TEST, inputBuf) == -1){
			fprintf(stderr, "Проблемы с отправкой команды на сервер. Соединение разорвано!\n");
			break;
		}

		execCommand(sock);
	}
	
	return 0;
}

void execCommand(int sock){
	struct Package package;
	int code = -1;
	for(;;){
		if(readPack(sock, &package) == -1) {
			fprintf(stderr, "Проблемы с получением ответа от сервера. Соединение разорвано!\n");
			break;
		}

		code = package.code;

		if(code == CODE_ERROR){
			fprintf(stderr, "Возникла ошибка!\n%s\n", package.data); fflush(stderr);
		} else if(code == CODE_INFO){
			fprintf(stdout, "%s\n", package.data);
		} else if(code == CODE_WAIT_FILE){
			sendFile(sock, package.data);
		} else if(code == CODE_SENDING_FILE){
			readFile(sock, package.data);
		} else if(code == CODE_OK){
			break;
		}
	}
}

void sendFile(int sock, char *fileName){

}

void readFile(int sock, char *fileName){

}