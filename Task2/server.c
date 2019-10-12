#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define SIZE_MSG 100

/*
TODO
Необходимые блокировки:
	чтение clients
	запись clients
	чтение clientQuantity
	запись clientQuantity
*/

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct Client {
    pthread_t threadId;
    int socket;
    char* address;
    int port;
    int number;
} *clients;
int clientQuantity = 0;


void initServerSocket(int *serverSocket, int port);
void* listenerConnetions(void* args);
void kickAllClients();
void kickClient(int kickNum);
void* clientHandler(void* args);
int readMessage(int socket);
int readCommand(int socket, char *cmdLine);
int readFile(int socket);
int readN(int socket, char* buf, int length);
void execCommand(char *cmd);


int main( int argc, char** argv) {

	if(argc != 3){
		fprintf(stderr, "%s\n%s\n", "Неверное количество аргументов!", "Необходим вызов: ./server [PORT] [WORK_PATH]");
		exit(1);
	}

	int port = *((int*) argv[1]);
	char *workPath = argv[2];
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
	int *serverSocket - ссылка на переменную сокета сервера
	int port - порт, на котором сервер будет слушать запрос на соединение	
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
        clients[indexClient].address = inet_ntoa(inetInfoAboutClient.sin_addr);
        clients[indexClient].port = inetInfoAboutClient.sin_port; //TODO возможно тут надо htons или что-то такое
        clients[indexClient].number = indexClient;

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
	int clientSocket = clients[indexClient].socket;
	//TODO убираем чтение
		pthread_mutex_unlock(&mutex);
	//----

	//TODO сделать динамическую память для каждого сообщения
		char msg[SIZE_MSG] = {0};
	//--
	fprintf(stdout, "Соединение с клиентом №%d установлено.\n", indexClient);
	for(;;) {
		if(readMessage(clientSocket) < 0){
			kickClient(indexClient);
			break;
		} else {
			fprintf(stdout, "Получил сообщение от клиента №%d.\n", indexClient);
			//TODO отправить клиенту какое-нибудь подтверждение о получении
		}
		memset(msg, 0, sizeof(msg));
	}

	fprintf(stdout, "Клиент №%d завершил своб работу.\n", indexClient);
}

/**
Функция читает сообщение из сокета-дескриптора. Поток будет заблокирован 
на этой функции, пока не будет считано сообщение или пока не произойдет
закрытие сокета.
Входные значения:
	int socket - сокет-дескриптор, из которого читается сообщение.
Возвращаемое значение:
	Количество считанных байт или -1 если не удалось считать сообщение.
*/
int readMessage(int socket){
	int resultBytes = 0;
	int result;
	char *cmdLine;
	result = readCommand(socket, cmdLine);
	if(result < 0){
		fprintf(stderr, "%s\n", "Ошибка в чтении команды!");
		return -1;
	}
	resultBytes += result;
	execCommand(cmdLine);



	sleep(5);
	return 1;
}

/**
Читает команду из сокета.
Входные значения:
	int socket - сокет-дескриптор, из которого читается команда.
	char *cmdLine - строка, в которую будет записана команда.
Возвращаемое значение:
	Количество прочитанных байт или -1 если не удалось считать команду.
*/
int readCommand(int socket, char *cmdLine){
	int size;
	if(readN(socket, &size, sizeof(size)) < 0){
		fprintf(stderr, "%s\n", "Не удалось считать длину команды!");
		return -1;
	}
	char msg[size]; //TODO ВОПРОС, не очиститься ли память, после окончания функции
	if(readN(socket, msg, size) < 0 ){
		fprintf(stderr, "%s\n", "Не удалось считать команду!");
		return -1;
	}
	cmdLine = msg;
	return size;
}

/**
Исполняет полученную команду, внутри себя вызывает функции проверки корретности команд.
Входные значения:
	char *cmd - строка, которая содержит команду.
*/
void execCommand(char *cmd){

}

int readFile(int socket){

}

/**
Функиця читает N байт из сокета. Поток будет заблокирован на этой функции, 
пока не будет считано заданное количество байт или пока не произойдет
закрытие сокета.
Входные значения:
	int socket - сокет-дескриптор, из которого будут считаны данные
	char *buf - строка, куда будут считаны данные
	int length - количество байт, которое необходимо считать
Возвращаемое значение:
	Количество считанных байт или -1 если не удалось считать данные.
*/
int readN(int socket, char* buf, int length){
	int result = 0;
	int readedBytes = 0;
	int sizeMsg = length;
	while(sizeMsg > 0){
		readedBytes = recv(socket, buf + result, sizeMsg, 0);
		if (readedBytes <= 0){
			return -1;
		}
		result += readedBytes;
		sizeMsg -= readedBytes;
	}
	return result;
}
