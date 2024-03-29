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
#include <dirent.h>
#include <sys/stat.h>

#include "declaration.h"
#include "dexchange.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct Command {
	int argc;
	char argv[MAX_ARG_IN_CMD][MAX_ARG_SIZE];
	char sourceCmdLine[SIZE_MSG];
};

struct Path{
	int count;
	char path[MAX_COUNT_DIR][MAX_LENGTH_FILE_NAME];
	char sourcePathLine[SIZE_MSG];
};

struct Client {
	pthread_t threadId;
	int socket;
	char address[SIZE_ADDR];
	int port;
	int number;
	struct Path dir;
} *clients;
int clientQuantity = 0;
char *rootDir;


void initServerSocket(int *serverSocket, int port);
void* listenerConnetions(void* args);
void kickAllClients();
void kickClient(int kickNum);
void* clientHandler(void* args);
int readFile(struct Client client, char *fileName, char *errorString);
int sendFile(struct Client client, char *fileName, char *errorString);
int execClientCommand(struct Client *client, char *cmdLine, char *errorString);
int execServerCommand(char *cmdLine, char *errorString);
int parseCmd(char *cmdLine, struct Command *cmd, char *errorString);
int validateCommand(struct Command cmd, char *errorString);
int sendListFilesInDir(struct Client client, char *errorString);
void makeDir(struct Path path, char *dirResult);
int changeClientDir(struct Client *client, char *path, char *errorString);
int isWho(char *path);


int main( int argc, char** argv) {

	if(argc != 3){
		fprintf(stderr, "%s\n%s\n", "Неверное количество аргументов!", "Необходим вызов: ./server [PORT] [WORK_PATH]");
		exit(1);
	}

	int port = *((int*) argv[1]);
	rootDir = argv[2];
	int serverSocket = -1;
	initServerSocket(&serverSocket, port);
    
	//Создание потока, который будет принимать входящие запросы на соединение
	pthread_t listenerThread;
    if (pthread_create(&listenerThread, NULL, listenerConnetions, (void*) &serverSocket)){
        fprintf(stderr, "%s\n", "Не удалось создать поток прослушивания подключений!");
        exit(1);
    }

	pthread_join(listenerThread, NULL);
	fprintf(stdout, "%s\n", "Сервер завершил работу.");
	
	return 0;
}

/**
Инициализация TCP сокета. Если произойдет какая-то ошибка, то программа будет
завершена вызовом функции exit().
Входные значения:
	int *serverSocket - ссылка на переменную сокета сервера;
	int port - порт, на котором сервер будет слушать запрос на соединение.
*/
void initServerSocket(int *serverSocket, int port){
    struct sockaddr_in listenerInfo;
	listenerInfo.sin_family = AF_INET;
	listenerInfo.sin_port = htons(port);
	listenerInfo.sin_addr.s_addr = htonl(INADDR_ANY);
	
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket < 0) {
		fprintf(stderr, "%s\n", "Не удалось создать сокет!");
		exit(1);
	}

	int enable = 1;
	if (setsockopt(serverSocket, IPPROTO_TCP, SO_REUSEADDR, &enable, sizeof(int)) < 0){
    	fprintf(stderr, "%s\n", "setsockopt(SO_REUSEADDR) failed!");
	}

	int resBind = bind(*serverSocket, (struct sockaddr *)&listenerInfo, sizeof(listenerInfo));
	if (resBind < 0 ) {
		fprintf(stderr, "%s\n", "Не удалось выполнить присваивание имени сокету!");
		exit(1);
	}
		
	if (listen(*serverSocket, 2)) {
		fprintf(stderr, "%s\n", "Не удалось выполнить listen!");
		exit(1);
	}

	fprintf(stdout, "%s\n", "Инициализация серверного сокета прошла успешно.");
}

/**
Функция ожидает подключения клиентов. Вызывается в новом потоке.
Входные значения:
	void* args - аргумент переданный в функцию при создании потока.
		В данном случае сюда приходит int *serverSocket - сокет-дескриптор сервера.
Возвращаемое значение:
	void* - так надо, чтоб вызывать функцию при создании потока.
*/
void* listenerConnetions(void* args){
    int serverSocket = *((int*) args);
    
    int clientSocket;
    struct sockaddr_in inetInfoAboutClient;
	int alen = sizeof(inetInfoAboutClient);
	fprintf(stdout, "%s\n", "Ожидание подключений запущено.");
    for(;;){

        clientSocket = accept(serverSocket, &inetInfoAboutClient, &alen);
        if (clientSocket <= 0){
        	fprintf(stderr, "%s\n", "Ожидание подключений прервано! Возможно сервер остановил свою работу.");
			kickAllClients();
      		break;
        }

        pthread_mutex_lock(&mutex);
        //TODO добавить функцию поиска свободного места и только если нет места, увеличивать размер
        	int indexClient = clientQuantity;
        	clients = (struct Client*) realloc(clients, sizeof(struct Client) * (clientQuantity + 1));
        //----
        clients[indexClient].socket = clientSocket;
        strcpy(clients[indexClient].address, inet_ntoa(inetInfoAboutClient.sin_addr));
        clients[indexClient].port = inetInfoAboutClient.sin_port; //TODO возможно тут надо htons или что-то такое
        clients[indexClient].number = indexClient;
        strcpy(clients[indexClient].dir.path[0], rootDir);
        clients[indexClient].dir.count = 1;

        if(pthread_create(&(clients[indexClient].threadId), NULL, clientHandler, (void*) &indexClient)) {
        	clients[indexClient].socket = -1; //Помечаем клиента, которого не удалось обработать как 'мертового'
            fprintf(stderr, "%s\n", "Не удалось создать поток для клиента, клиент не будет обрабатываться!");
            continue;
        }
        
        clientQuantity++;
        pthread_mutex_unlock(&mutex);
    }

    pthread_mutex_lock(&mutex);
    for (int i = 0; i < clientQuantity; i++){
    	pthread_join(clients[i].threadId, NULL);
    }
    pthread_mutex_unlock(&mutex);

    fprintf(stdout, "%s\n", "Ожидание подключений завершено.");
    free(clients);
}

/**
Функция отключает всех клиентов.
*/
void kickAllClients(){
	pthread_mutex_lock(&mutex);
	int count = clientQuantity;
    pthread_mutex_unlock(&mutex);
    for(int i = 0; i < count; i++){
    	kickClient(i);
    }
    fprintf(stdout, "%s\n", "Отключение всех клиентов завершено.");
}

/**
Функция отключает клиента.
Входные значения:
	int kickNum - номер отключаемого клиента
*/
void kickClient(int kickNum){
	pthread_mutex_lock(&mutex);
	close(clients[kickNum].socket);
	clients[kickNum].socket = -1;
    pthread_mutex_unlock(&mutex);
    fprintf(stdout, "Кажись клиент №%d был отключен.\n", kickNum);
}

/**
Функция обрабокти клиента. Вызывается в новом потоке.
Входные значения:
	void* args - аргумент переданный в функцию при создании потока.
		В данном случае сюда приходит int *indexClient - номер клиента.
Возвращаемое значение:
	void* - так надо, чтоб вызывать функцию при создании потока.
*/
void* clientHandler(void* args){
	int indexClient = *((int*)args);
	pthread_mutex_lock(&mutex);
	struct Client *client = &(clients[indexClient]);
	int clientSocket = client->socket;
	pthread_mutex_unlock(&mutex);

	fprintf(stdout, "Соединение с клиентом №%d установлено.\n", indexClient);

	struct Package package;
	char errorString[SIZE_MSG];
	for(;;) {
		bzero(errorString, sizeof(errorString));
		if(readPack(clientSocket, &package) < 0){
			kickClient(indexClient);
			break;
		} else {
			if(package.code == CODE_CMD){
				int err = execClientCommand(client, package.data, errorString);
				if(err == -1){
					sendPack(clientSocket, CODE_ERROR, strlen(errorString) + 1, errorString);
				}
			} else {
				fprintf(stderr, "Ожидался пакет с кодом - %d, получили пакет с кодом - %d\n", CODE_CMD, package.code);
				sprintf(errorString, "%s\n", "Ожидалась команда!");
				sendPack(clientSocket, CODE_ERROR, strlen(errorString) + 1, errorString);
			}
			sendPack(clientSocket, CODE_OK, strlen("Команда обработана.") + 1, "Команда обработана.");
		}
	}

	fprintf(stdout, "Клиент №%d завершил своб работу.\n", indexClient);
}

/**
Исполняет полученную команду, внутри себя вызывает функции проверки корретности команд.
Обрабатываемые команды от клиентов:
	ls - отправляет пользоватлею список файлов в директории;
	cd [DIR] - навигация по директориям;
	load [FILE_NAME] - загружает файл на сервер;
	dload [FILE_NAME] - отправляет файл пользователю.
Входные значения:
	struct Client *client - структура, которая содержит информацию о клиенте;
	char *cmdLine - строка, которая содержит исходную команду;
	char *errorString - строка, которая содержит описание ошибки.
Возвращаемое значение:
	1 - команда выполнена или -1 елси возникла ошибка. Описание ошибки содержиться в 
	переменной errorString.
*/
int execClientCommand(struct Client *client, char *cmdLine, char *errorString){
	bzero(errorString, sizeof(errorString));
	struct Command cmd;
	if(parseCmd(cmdLine, &cmd, errorString) == -1){
		fprintf(stderr, "Не удалось распарсить команду клиента: %s\nОписание ошибки: %s\n", cmdLine, errorString);
		return -1;
	}
	if(validateCommand(cmd, errorString) == -1){
		fprintf(stderr, "Команда клиента: %s - неккоретна!\nОписание ошибки: %s\n", cmdLine, errorString);
		return -1;
	}
	fprintf(stdout, "Команда клиента: %s - корректна.\n", cmdLine);
	pthread_mutex_lock(&mutex);
	struct Client clientCopy = *client;
	pthread_mutex_unlock(&mutex);
	if(!strcmp(cmd.argv[0], "ls")) {
		return sendListFilesInDir(clientCopy, errorString);
	} else if(!strcmp(cmd.argv[0], "cd")) {
		return changeClientDir(client, cmd.argv[1], errorString);
	} else if(!strcmp(cmd.argv[0], "load")) {
		return readFile(clientCopy, cmd.argv[1], errorString); //TODO
	} else if(!strcmp(cmd.argv[0], "dload")){
		return sendFile(clientCopy, cmd.argv[1], errorString); //TODO
	} else {
		sprintf(stderr, "Хоть мы все и проверили, но что-то с ней не так: %s\n", cmdLine);
		return -1;
	}
	return 1;
}

/**
Отправляет клиенту список файлов и директорий, которые находятся в текущем каталоге.
Входные значения:
	struct Client client - информация о клиенте;
	char *errorString - строка, которое содержит описание ошибки.
Возвращаемое значение:
	1 если все хорошо или -1 если произошла ошибка. Описание ошибки содержиться в 
	переменной errorString.
*/
int sendListFilesInDir(struct Client client, char *errorString){
	char dirStr[SIZE_MSG] = {'\0'};
	makeDir(client.dir, dirStr);
	DIR *dir = opendir(dirStr);
	if(dir == NULL){
		fprintf(stdout, "Не смог открыть директорию - %s\n", dirStr);
		sprintf(errorString, "Не удалось получить список файлов из директории - %s. Возможно она не существует.\n", dirStr);
		return -1;
	}
	struct dirent *dirent;
	char fname[SIZE_MSG] = {0};
	while((dirent = readdir(dir)) != NULL){
		if(dirent->d_name[0] == '.'){
			continue;
		}
		strcpy(fname, dirent->d_name);
		if(sendPack(client.socket, CODE_INFO, strlen(fname) + 1, fname) == -1){
			fprintf(stdout, "Проблема с отправкой имени файла из директории - %s\n", dirStr);
			sprintf(errorString, "Не получилось отправить навзание файла из директории - %s", dirStr);
			return -1;
		}
		bzero(fname, sizeof(fname));
	}
	if(closedir(dir) == -1){
		fprintf(stdout, "Беда! Не могу закрыть директорию - %s", dirStr);
		sprintf(stderr, "Дичь, очень сранная проблема.");
		return -1;
	}
	return 1;
}

/**
Собирает полный путь в одну строку.
Входные значения:
	sruct Path path - путь к директории;
	char *dirResult - строка, в которую будет записан путь.
*/
void makeDir(struct Path path, char *dirResult){
	int count = path.count;
	for(int i = 0; i < count - 1; i++){
		strcat(dirResult, path.path[i]);
		strcat(dirResult, "/");
	}
	strcat(dirResult, path.path[count - 1]);
}

/**
Изменение директории клиента.
Входные значения:
	struct Client *client - информация о клиенте;
	char *path - целевой каталог;
	char *errorString - строка, которая содержит описание ошибки.
Возвращаемое значение:
	1 если все хорошо или -1 если произошла ошибка. Описание ошибки содержитсья в
	переменной errorString.
*/
int changeClientDir(struct Client *client, char *path, char *errorString){
	pthread_mutex_lock(&mutex);
	int count = client->dir.count;
	pthread_mutex_unlock(&mutex);
	if(!strcmp(path, "..")){
		if(count == 1){
			sendPack(client->socket, CODE_INFO, strlen("Вы находитесь в корневой директории.") + 1, "Вы находитесь в корневой директории.");
		} else {
			pthread_mutex_lock(&mutex);
			bzero(client->dir.path[count - 1], sizeof(client->dir.path[count - 1]));
			client->dir.count--;
			pthread_mutex_unlock(&mutex);
		}
	} else {
		char tempPath[SIZE_MSG] = {0};
		pthread_mutex_lock(&mutex);
		struct Path t = client->dir;
		pthread_mutex_unlock(&mutex);
		makeDir(t, tempPath);
		strcat(tempPath, "/");
		strcat(tempPath, path);
		fprintf(stdout, "вот - %s\n", tempPath);
		if(isWho(tempPath) != 2){
			sprintf(errorString, "%s - это не каталог!", path);
			return -1;
		}
		int err = parsePath(path, &(client->dir), errorString);
		if(err == -1){
			return -1;
		}
	}
	char clientDir[SIZE_MSG] = {0};
	strcat(clientDir, "~/");
	pthread_mutex_lock(&mutex);
	count = client->dir.count;
	if(count > 1){
		for(int i = 1; i < count - 1; i++){
			strcat(clientDir, client->dir.path[i]);
			strcat(clientDir, "/");
		}
		strcat(clientDir, client->dir.path[count - 1]);
	}
	pthread_mutex_unlock(&mutex);
	strcat(clientDir, "$");
	sendPack(client->socket, CODE_YOUR_PATH, strlen(clientDir) + 1, clientDir);
	return 1;
}

/**
Проверяет, что находится по данному пути файл или папка.
Вхоные значения:
	char *path - путь к каталогу.
Возвращаемое значение:
	1 если это файл, 2 если это папка или -1 если что-то не так.
*/
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

/**
Исполняет полученную команду, внутри себя вызывает функции проверки корретности команд.
Обрабатываемые собственные команды:
	/kick [NUMBER] - принудительно отключает клиента;
	/lclients - список подключенных клиентов;
	/shutdown - завершает работу сервера.
Входные значения:
	char *cmdLine - строка, которая содержит команду;
	char *errorString - строка, которая будет содержать сообщение ошибки.
Возвращаемое значение:
	1 если команда не влияет на работу сервера, 0 если пора завершать работу
	или -1 если произошла какая-то ошибка. Описание ошибки содержится в
	переменной errorString.
*/
int execServerCommand(char *cmdLine, char *errorString){ //TODO
	bzero(errorString, sizeof(errorString));
	struct Command cmd;
	if(parseCmd(cmdLine, &cmd, errorString) == -1){
		fprintf(stderr, "Не удалось распарсить команду сервера: %s\nОписание ошибки: %s\n", cmdLine, errorString);
		return -1;
	}
	if(validateCommand(cmd, errorString) == -1){
		fprintf(stderr, "Команда сервера: %s - неккоретна!\nОписание ошибки: %s\n", cmdLine, errorString);
		return -1;
	}
	fprintf(stdout, "Команда сервера: %s - корректна.\n", cmdLine);
	if(!strcmp(cmd.argv[0], "/kick")){
		kickClient(atoi(cmd.argv[1]));
	} else if(!strcmp(cmd.argv[0], "/shutdown")){
		return 0;
	} else {
		sprintf(errorString, "Хоть мы все ипроверили, но что-то с командой не так: %s\n", cmdLine);
		return -1;
	}
	return 1;
}

/**
Функция парсит входную строку, на аргументы.
Входные значения:
	char *cmdLine - строка, которая содержит исходную команду;
	struct Command *cmd - структура, в которую будет записана информация о команде;
	char *errorString - строка, которая соодержит описание.
Возвращаемое значение:
	1 если все хорошо или -1 если произошла ошибка. Описание ошибки содержится в переменной 
	errorString.
*/
int parseCmd(char *cmdLine, struct Command *cmd, char *errorString){
	bzero(errorString, sizeof(errorString));
	int countArg = 0;
	char *sep = " ";
	char *arg = strtok(cmdLine, sep);
	fprintf(stdout, "Сари - %s\n", arg);
	if(arg == NULL){
		sprintf(errorString, "Команда: %s - не поддается парсингу.\nВведите корректную команду. Используйте: help\n", cmdLine);
		return -1;
	}
	while(arg != NULL && countArg <= MAX_ARG_IN_CMD){
		countArg ++;
		strcpy(cmd->argv[countArg - 1], arg);
		arg = strtok(NULL, sep);
	}
	if(countArg > MAX_ARG_IN_CMD){
		sprintf(errorString, "Слишком много аргументов в команде: %s - таких команд у нас нет. Используйте: help\n", cmdLine);
		return -1;
	}
	cmd->argc = countArg;
	strcpy(cmd->sourceCmdLine, cmdLine);
	return 1;
}

/**
Парсинг пути к директории. Сохранение каждого католога, как отдельный элемент массива.
Входные значения:
	cgar *pathLine - строка, которая содержит исходный путь к директории;
	struct Path *path - структура, в которую будет записана информация о пути;
	char *errorString - строка, которая содержит описание ошибки.
Возвращаемое значение:
	1 если все хорошо или -1 если произошла ошибка. Описание ошибки содержится в переменной errorString.
*/
int parsePath(char *pathLine, struct Path *path, char *errorString){
	bzero(errorString, sizeof(errorString));
	int countArg = path->count;
	if(pathLine[0] == '/'){
		sprintf(errorString, "Неккоретный путь!");
		return -1;
	}
	char *sep = "/";
	char *arg = strtok(pathLine, sep);
	if(arg == NULL){
		sprintf(errorString, "Неверный путь: %s\nИспользуйте: path/to/dir\n", pathLine);
		return -1;
	}
	while(arg != NULL && countArg <= MAX_COUNT_DIR){
		countArg++;
		strcpy(path->path[countArg - 1], arg);
		arg = strtok(NULL, sep);
	}
	if(countArg > MAX_COUNT_DIR){
		sprintf(errorString, "Слишком много вложенных директорий. MAX = %d\n", MAX_COUNT_DIR);
		return -1;
	}
	path->count = countArg;
	strcpy(path->sourcePathLine, pathLine);
	return 1;
}

/**
Проверка корректности команды.
Входные значения:
	struct Command cmd - структура, которая содержит информацию о команде;
	char *errorString - строка, которая содержит описание ошибки.
Возвращаемое значение:
	1 если все хорошо или -1 если произошла ошибка. Описание ошибки содержится
	в переменной errorString.
*/
int validateCommand(struct Command cmd, char *errorString){
	bzero(errorString, sizeof(errorString));
	int argc = cmd.argc;
	char *firstArg = cmd.argv[0];
	char *cmdLine = cmd.sourceCmdLine;
	if(!strcmp(firstArg, "ls")){
		if(argc != 1){
			sprintf(errorString, "Команда: %s - имеет лишние аргументы. Воспользуейтесь командой: help\n", cmdLine);
			return -1;
		}
	} else if(!strcmp(firstArg, "cd")){
		if(argc != 2){
			sprintf(errorString, "Команда: %s - имеет много или мало аргументов. Воспользуейтесь командой: help\n", cmdLine);
			return -1;
		}
	} else if(!strcmp(firstArg, "load")){
		if(argc != 2){
			sprintf(errorString, "Команда: %s - имеет много или мало аргументов. Воспользуейтесь командой: help\n", cmdLine);
			return -1;
		}
	} else if(!strcmp(firstArg, "dload")){
		if(argc != 2){
			sprintf(errorString, "Команда: %s - имеет много или мало аргументов. Воспользуейтесь командой: help\n", cmdLine);
			return -1;
		}
	} else if(!strcmp(firstArg, "/kick")){
		if(argc != 2){
			sprintf(errorString, "Команда: %s - имеет много или мало аргументов. Воспользуейтесь командой: help\n", cmdLine);
			return -1;
		}
		char *pattern = "^\\d+$";
		regex_t preg;
    	int err,regerr;
    	err = regcomp (&preg, pattern, REG_EXTENDED);
    	if(err != 0){
    		fprintf(stdout, "Не получилось скомпилировать регулярное выражение: %s\n", pattern);
    		sprintf(errorString, "С командой: %s - что-то не так. Воспользуейтесь командой: help\n", cmdLine);
    		return -1;
    	}
    	regmatch_t pm;
    	regerr = regexec (&preg, cmd.argv[1], 0, &pm, 0);
    	if(regerr != 0){
    		sprintf(errorString, "Команда: %s - имеет некорретные аргументы. Воспользуейтесь командой: /help\n", cmdLine);
    		return -1;
    	}
	} else if(!strcmp(firstArg, "/shutdown")){
		if(argc!= 1){
			sprintf(errorString, "Команда: %s - имеет лишние аргументы! Воспользуейтесь командой: /help\n", cmdLine);
			return -1;
		}
	} else {
		sprintf(errorString, "Неизвстная команда: %s. Воспользуейтесь командой: help\n", cmdLine);
		return -1;
	}
	return 1;
}

/**
Получает файл от клиента.
Входные значения:
	struct Client client - информация о клиенте;
	char *fileName - путь к файлу, на стороне клиента;
	char *errorString - описание ошибки.
Возвращаемое значение:
	1 если все хорошо или -1 если произошла ошибка. Описание ошибки содержиться в
	переменной errorString.
*/
int readFile(struct Client client, char *fileName, char *errorString){
	sendPack(client.socket, CODE_REQUEST_FILE, strlen(fileName) + 1, fileName);
	char *sep = "/";
	char *temp = strtok(fileName, sep);
	char fName[SIZE_MSG];
	while(temp != NULL){
		bzero(fName, sizeof(fName));
		strcpy(fName, temp);
		temp = strtok(NULL, sep);
	}

	char dirStr[SIZE_MSG] = {0};
	makeDir(client.dir, dirStr);
	strcat(dirStr, "/");
	strcat(dirStr, fName);

	FILE *file = fopen(dirStr, "wb");
	if(file == NULL){
		fprintf(stderr, "Не удалось открыть файл: %s - для записи!\n", dirStr);
		sprintf(errorString, "Не удалось загрузить файл - %s.", fName);
		return -1;
	}

	struct Package package;
	int err;
	for(;;){
		err = readPack(client.socket, &package);
		if(err == -1){
			fprintf(stdout, "Не удалось принять кусок файла - %s. Данные не были сохранены.\n", fName);
			sprintf(errorString, "Не удалось загрузить файл - %s.", fName);
			fclose(file);
			remove(dirStr);
			return -1;
		}
		if(package.code == CODE_FILE_SECTION){
			fprintf(stdout, "Пишу байт - %d\n", package.sizeData);
			fwrite(package.data, sizeof(char), package.sizeData, file);
		} else if(package.code == CODE_FILE_END){
			fprintf(stdout, "Файл: %s - получен.\n", fName);
			break;
		} else {
			fprintf(stderr, "Пришел неправильный пакет с кодом - %d.\n", package.code);
			sprintf(errorString, "Не удалось загрузить файл - %s.", fName);
			fclose(file);
			remove(dirStr);
			return -1;
		}
	}
	fclose(file);
	sendPack(client.socket, CODE_INFO, strlen("Файл загружен.\n") + 1, "Файл загружен.\n");
	return 1;
}

/**
Отправка файла клиенту.
Входные значения:
	struct Client client - информация о клиенте;
	char *fileName - имя файла;
	char *errorString - описание ошбики.
Возвращаемое значение:
	1 если все хорошо или -1 если произошла ошибка. Описание ошибки содержиться в
	переменной errorString.
*/
int sendFile(struct Client client, char *fileName, char *errorString){
	bzero(errorString, sizeof(errorString));
	char dirStr[SIZE_MSG] = {0};
	makeDir(client.dir, dirStr);
	strcat(dirStr, "/");
	strcat(dirStr, fileName);

	if(isWho(dirStr) != 1){
		sprintf(errorString, "%s - это не файл!", fileName);
		printf("AAAAA\n"); fflush(stdout);
		return -1;
	}

	sendPack(client.socket, CODE_RESPONSE_FILE, strlen(fileName) + 1, fileName);

	FILE *file = fopen(dirStr, "rb");
	if(file == NULL){
		fprintf(stderr, "Не удалось открыть файл: %s - для записи!\n", dirStr);
		sprintf(errorString, "Не удалось отправить файл - %s.", fileName);
		return -1;
	}
	char section[SIZE_MSG] = {'\0'};
	int res = 0, err;
	while((res = fread(section, sizeof(char), sizeof(section), file)) != 0){
		err = sendPack(client.socket, CODE_FILE_SECTION, res, section); //TODO обработать ошибку
		if(err == -1){
			fprintf(stderr, "Не удалось отправить кусок файла - %s\n", fileName);
			sprintf(errorString, "Не удалось отправить файл - %s\n", fileName);
			fclose(file);
			return -1;
		}
		bzero(section, sizeof(section));
	}
	fclose(file);
	sendPack(client.socket, CODE_FILE_END, strlen("Файл отправлен полностью.") + 1, "Файл отправлен полностью.");
	return 1;
}
