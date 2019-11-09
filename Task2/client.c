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
#include <sys/stat.h>

/*
ВОПРОСЫ:
	что если сделать const char *str на входе функции?
	конкурентный подход обращения к файлам? (копии создавать) man 2 flock
	отключение во время передачи файла?
	ошибки во время передачи файла?
	обращение в файлам по пути или только по имени в текущей директроии?
*/

#include "declaration.h"
#include "dexchange.h"

int execCommand(int sock);
void sendFile(int sock, char *fileName);
void readFile(int sock, char *fileName);

char workDir[SIZE_MSG];

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
	
	bzero(workDir, sizeof(workDir));
	strcpy(workDir, "~$");

	char inputBuf[SIZE_MSG];
	for(;;){
		fprintf(stdout, "%s", workDir);
		bzero(inputBuf, sizeof(inputBuf));
		fgets(inputBuf, sizeof(inputBuf), stdin);
		inputBuf[strlen(inputBuf) - 1] = '\0';

		if(!strcmp("quit", inputBuf)){
			close(sock);
			break;
		}

		if(sendPack(sock, CODE_CMD, strlen(inputBuf) + 1, inputBuf) == -1){
			fprintf(stderr, "Проблемы с отправкой команды на сервер. Соединение разорвано!\n");
			break;
		}

		if(execCommand(sock) == 0){
			break;
		}
	}
	
	return 0;
}

int execCommand(int sock){
	struct Package package;
	int code = -1;
	for(;;){
		if(readPack(sock, &package) == -1) {
			fprintf(stderr, "Проблемы с получением ответа от сервера. Соединение разорвано!\n");
			return 0;
		}

		code = package.code;

		if(code == CODE_ERROR){
			fprintf(stderr, "Возникла ошибка!\n%s\n", package.data);
		} else if(code == CODE_INFO){
			fprintf(stdout, "%s\n", package.data);
		} else if(code == CODE_REQUEST_FILE){
			sendFile(sock, package.data);
		} else if(code == CODE_RESPONSE_FILE){
			readFile(sock, package.data);
		} else if(code == CODE_YOUR_PATH){
			bzero(workDir, sizeof(workDir));
			strcpy(workDir, package.data);
		} else if(code == CODE_OK){
			break;
		}
	}
	return 1;
}

int isWho(char *path){
	struct stat statBuf;
  	if(stat(path, &statBuf) == -1){
  		return -1;
  	}
  	if(S_ISREG(statBuf.st_mode)){
  	  return 1;
  	}else if(S_ISDIR(statBuf.st_mode)){
  	  return 2;
  	}else if(S_ISLNK(statBuf.st_mode)){
  	  return 1;
  	} else {
  		return -1;
  	}
}

//TODO не давать передавать директорию
void sendFile(int sock, char *fileName){
	if(isWho(fileName) != 1){
		fprintf(stderr, "%s - это не файл!", fileName);
		sendPack(sock, CODE_ERROR, strlen("Отмена.") + 1, "Отмена.");
		return;
	}
	FILE *file = fopen(fileName, "rb");
	if(file == NULL){
		fprintf(stderr, "Не удалось загрузить файл - %s\n", fileName);
		sendPack(sock, CODE_ERROR, strlen("Отмена.") + 1, "Отмена.");
		return;
	}
	char section[SIZE_MSG] = {'\0'};
	int res = 0, err;
	while((res = fread(section, sizeof(char), sizeof(section), file)) != 0){
		fprintf(stdout, "res = %d\n", res);
		err = sendPack(sock, CODE_FILE_SECTION, res, section); //TODO обработать ошибку
		if(err == -1){
			fprintf(stderr, "Не удалось отправить файл - %s\n", fileName);
			fclose(file);
			return;
		}
		bzero(section, sizeof(section));
	}
	fclose(file);
	sendPack(sock, CODE_FILE_END, strlen("Файл отправлен полностью.") + 1, "Файл отправлен полностью.");
}

void readFile(int sock, char *fileName){
	char *sep = "/";
	char *temp = strtok(fileName, sep);
	char fName[SIZE_MSG];
	while(temp != NULL){
		bzero(fName, sizeof(fName));
		strcpy(fName, temp);
		temp = strtok(NULL, sep);
	}

	FILE *file = fopen(fName, "wb");
	if(file == NULL){
		fprintf(stderr, "Не удалось скачать файл - %s\n", fName);
		sendPack(sock, CODE_ERROR, strlen("Отмена") + 1, "Отмена");
		return;
	}

	struct Package package;
	int err;
	for(;;){
		err = readPack(sock, &package);
		if(err == -1){
			fprintf(stderr, "Не удалось принять кусок файла - %s. Данные не были сохранены.\n", fName);
			fclose(file);
			remove(fName);
			return;
		}
		if(package.code == CODE_FILE_SECTION){
			fwrite(package.data, sizeof(char), package.sizeData, file);
		} else if(package.code == CODE_FILE_END){
			fprintf(stdout, "Файл: %s - скачан.\n", fName);
			break;
		} else {
			fprintf(stderr, "Не удалось принять кусок файла - %s. Данные не были сохранены.\n", fName);
			fclose(file);
			remove(fName);
			return;
		}
	}
	fclose(file);
}
