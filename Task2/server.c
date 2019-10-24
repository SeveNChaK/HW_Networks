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

#include "declaration.h"
#include "dexchange.h"

/*
ВОПРОСЫ
	- Копирование структуры?
*/

/*
TODO
Необходимые блокировки:
	чтение clients
	запись clients
	чтение clientQuantity
	запись clientQuantity
*/

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
}

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
int readFile(int socket);
int execClientCommand(int clientSocket, char *cmdLine, char *errorString);
int execServerCommand(char *cmdLine, char *errorString);
int parseCmd(char *cmdLine, struct Command *cmd, char *errorString);
int validateCommand(struct Command cmd, char *errorString);


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
    
	//Цикл чтения ввода с клавиатуры TODO фиксить
 //    printf("Input (/help to help): \n"); fflush(stdout);
 //    char buf[100];
	// for(;;) {
	// 	bzero(buf, 100);
	// 	fgets(buf, 100, stdin);
	// 	buf[strlen(buf) - 1] = '\0';
		
	// 	if(!strcmp("/help", buf)){
	// 		printf("HELP:\n");
	// 		printf("\'/lclients\' to list users on-line;\n");
	// 		printf("\'/kick [number client]\' to kick client from server;\n");
	// 		printf("\'/quit\' to shutdown;\n");
	// 		fflush(stdout);
	// 	} else if(!strcmp("/lclients", buf)){
	// 			printf("Clients on-line:\n");
	// 			printf(" NUMBER      ADDRESS            PORT\n");

	// 			pthread_mutex_lock(&mutex);
	// 			for(int i = 0; i < clientQuantity; i++){
	// 				if(clients[i].socket != -1)
	// 					printf("  %d       %s        %d\n", clients[i].number, clients[i].address, clients[i].port);
	// 			}
	// 			pthread_mutex_unlock(&mutex);

	// 			fflush(stdout);
	// 	} else if(!strcmp("/quit", buf)) {
	// 			shutdown(listener, 2);
	// 			close(listener);
	// 			pthread_join(listenerThread, NULL);
	// 			break;
	// 	} else {
	// 		char *sep = " ";
	// 		char *str = strtok(buf, sep);
	// 		if(str == NULL) {
	// 			printf("Illegal format!\n"); fflush(stdout);
	// 			continue;
	// 		}
	// 		if(!strcmp("/kick", str)){
	// 			str = strtok(NULL, sep);
	// 			int kickNum = atoi(str);
	// 			if(str[0] != '0' && kickNum == 0){
	// 				printf("Illegal format!\n"); fflush(stdout);
	// 				continue;
	// 			}
	// 			kickClient(kickNum);
	// 		}
	// 	}
	// }

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
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
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

        //TODO провераем чтение (оба), ставим запись (оба)
        	pthread_mutex_lock(&mutex);
        //----
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
        //TODO убираем запись (оба)
        	pthread_mutex_unlock(&mutex);
        //----
        
        
        //TODO провераем чтение, ставим запись
        clientQuantity++;
        //TODO убираем запись
    }

    //TODO проверяем запись (оба), ставим чтение (оба)
    for (int i = 0; i < clientQuantity; i++){
    	pthread_join(clients[i].threadId, NULL);
    }
    //TODO убираем чтение (оба)

    fprintf(stdout, "%s\n", "Ожидание подключений завершено.");
    free(clients);
}

/**
Функция отключает всех клиентов.
*/
void kickAllClients(){
	//TODO проверяем запись, ставим чтение
		pthread_mutex_lock(&mutex);
	//----
	int count = clientQuantity;
	//TODO убираем чтение
    	pthread_mutex_unlock(&mutex);
    //----
    for(int i = 0; i < count; i++){ //Отключаем всех клиентов
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
	//TODO проверяем чтение, ставим запись
		pthread_mutex_lock(&mutex);
	//----
	close(clients[kickNum].socket);
	clients[kickNum].socket = -1;
	//TODO убираем запись
    	pthread_mutex_unlock(&mutex);
    //----
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
	//TODO проверяем запись, стави чтение
		pthread_mutex_lock(&mutex);
	//----
	struct Client *client = clients[indexClient];
	int clientSocket = client.socket;
	//TODO убираем чтение
		pthread_mutex_unlock(&mutex);
	//----

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
					sendPack(clientSocket, CODE_ERROR, errorString);
				}
			} else {
				fprintf(stderr, "Ожидался пакет с кодом - %d, получили пакет с кодом - %d\n", CODE_CMD, package.code);
				sprintf(errorString, "%s\n", "Ожидалась команда!");
				sendPack(clientSocket, CODE_ERROR, errorString);
			}
			sendPack(clientSocket, CODE_OK, "Команда обработана.");
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
	get [FILE_NAME] - отправляет файл пользователю.
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
	struct CLient clientCopy = *client;
	if(!strcmp(cmd.argv[0], "ls")) {
		return sendListFilesInDir(*client, errorString); //TODO
	} else if(!strcmp(cmd.argv[0], "cd")) {
		return changeClientDir(client, cmd->argv[1], errorString); //TDOO
	} else if(!strcmp(cmd.argv[0], "load")) {
		return readFile(*client, cmd->argv[1], errorString); //TODO
	} else if(!strcmp(cmd.argv[0], "get")){
		return sendFile(*client, cmd->argv[1], errorString); //TODO
	} else {
		sprintf(stderr, "Хоть мы все и проверили, но что-то с ней не так: %s\n", cmdLine);
		return -1;
	}
	return 1;
}

/**
Исполняет полученную команду, внутри себя вызывает функции проверки корретности команд.
Обрабатываемые собственные команды:
	/kick [NUMBER] - принудительно отключает клиента;
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
	char *arg = strtok(sep, cmdLine);
	if(arg == NULL){
		sprintf(errorString, "Команда: %s - не поддается парсингу.\nВведите корректную команду. Используйте: help\n", cmdLine);
		return -1;
	}
	while(arg != NULL && countArg <= MAX_ARG_IN_CMD){
		countArg ++;
		strcpy(cmd->argv[countArg - 1], arg);
		arg = strtok(NULL, cmdLine);
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
	char *sep = "/";
	char *arg = strtok(sep, pathLine);
	if(arg == NULL){
		sprintf(errorString, "Неверный путь: %s\nИспользуйте: path/to/dir\n", pathLine);
		return -1;
	}
	while(arg != NULL && countArg <= MAX_COUNT_DIR){
		countArg++;
		strcpy(path->path[countArg - 1], arg);
		arg = strtok(NULL, pathLine);
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
	} else if(!strcmp(firstArg, "get")){
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
*/
int readFile(int socket){

}
