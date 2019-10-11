#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define PORT 62321 //Порт сервера
#define SIZE_MSG 100

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct tInfo {
    pthread_t threadId;
    int socket;
    char* address;
    int port;
    int number;
} *clients;
int clientQuantity = 0;

void* clientHandler(void* args);
void* listenerConnetions(void* args);
void* kickClient(int kickNum);
int readN(int socket, char* buf);


int main( int argc, char** argv) {

//Серверный сокет
    struct sockaddr_in listenerInfo;
	listenerInfo.sin_family = AF_INET;
	listenerInfo.sin_port = htons(PORT);
	listenerInfo.sin_addr.s_addr = htonl(INADDR_ANY); //INADDR_ANY INADDR_LOOPBACK
	
	int listener = socket(AF_INET, SOCK_STREAM, 0 ); //Прослушивающий сокет
	if ( listener < 0 ) {
		perror( "Can't create socket to listen: ");
		exit(1);
	}
	printf("Прослушивающий сокет создан.\n");
	fflush(stdout);
	
	int resBind = bind(listener, (struct sockaddr *)&listenerInfo, sizeof(listenerInfo));
	if (resBind < 0 ) {
		perror( "Can't bind socket" );
		exit(1);
	}
	printf("Прослушивающий сокет забинден.\n");
	fflush(stdout);
		
	if ( listen(listener, 2) ) { //Слушаем входящие соединения
		perror("Error while listening: ");
		exit(1);
	}
	printf("Начинаем слушать входящие соединения...\n");
	fflush(stdout);
//------------------------------------------------------
    
//Создание потока, который будет принимать входящие запросы на соединение
	pthread_t listenerThread;
    if (pthread_create(&listenerThread, NULL, listenerConnetions, (void*) &listener)){
        printf("ERROR: Can't create listener thread!"); fflush(stdout);
        exit(1);
    }
    
//Цикл чтения ввода с клавиатуры
    printf("Input (/help to help): \n"); fflush(stdout);
    char buf[100];
	for(;;) {
		bzero(buf, 100);
		fgets(buf, 100, stdin);
		buf[strlen(buf) - 1] = '\0';
		
		if(!strcmp("/help", buf)){
			printf("HELP:\n");
			printf("\'/lclients\' to list users on-line;\n");
			printf("\'/kick [number client]\' to kick client from server;\n");
			printf("\'/quit\' to shutdown;\n");
			fflush(stdout);
		} else if(!strcmp("/lclients", buf)){
				printf("Clients on-line:\n");
				printf(" NUMBER      ADDRESS            PORT\n");

				pthread_mutex_lock(&mutex);
				for(int i = 0; i < clientQuantity; i++){
					if(clients[i].socket != -1)
						printf("  %d       %s        %d\n", clients[i].number, clients[i].address, clients[i].port);
				}
				pthread_mutex_unlock(&mutex);

				fflush(stdout);
		} else if(!strcmp("/quit", buf)) {
				shutdown(listener, 2);
				close(listener);
				pthread_join(listenerThread, NULL);
				break;
		} else {
			char *sep = " ";
			char *str = strtok(buf, sep);
			if(str == NULL) {
				printf("Illegal format!\n"); fflush(stdout);
				continue;
			}
			if(!strcmp("/kick", str)){
				str = strtok(NULL, sep);
				int kickNum = atoi(str);
				if(str[0] != '0' && kickNum == 0){
					printf("Illegal format!\n"); fflush(stdout);
					continue;
				}
				kickClient(kickNum);
			}
		}

	}

	printf("ENDED SERVER!\n"); fflush(stdout);
	
	return 0;
}

//Обработка одного клиента
void* clientHandler(void* args){
	
	pthread_mutex_lock(&mutex);
	int index = *((int*)args);
	int sock = clients[index].socket;
	pthread_mutex_unlock(&mutex);

	char msg[SIZE_MSG] = {0};
	for(;;) {
		if (readN(sock, msg) <= 0) {
			printf("Client №%d disconnect\n", index); fflush(stdout);
			shutdown(sock, 2);
			close(sock);
			//pthread_mutex_lock(&mutex);
			//clients[index].socket = -1;
			//pthread_mutex_unlock(&mutex);
			break;
		} else {
			printf("Get message: %s\n", msg); fflush(stdout);
			send(sock, msg, sizeof(msg), 0);
		}
		memset(msg, 0, sizeof(msg));
	}

	printf("ENDED CLIENT №%d!\n", index); fflush(stdout);
}

void* listenerConnetions(void* args){
    int listener = *((int*) args);
    
    int s;
    int indexClient;
    struct sockaddr_in a;
	int alen = sizeof(a);
    for(;;){

        s = accept(listener, &a, &alen);
        if (s <= 0){
        	printf("STOPING SERVER...\n"); fflush(stdout);
			
			pthread_mutex_lock(&mutex);
    		for(int i = 0; i < clientQuantity; i++){
        		shutdown(clients[i].socket, 2);
        		close(clients[i].socket);
        		clients[i].socket = -1;
    		}
    		for (int i = 0; i < clientQuantity; i++){
    			pthread_join(clients[i].threadId, NULL);
    		}
    		pthread_mutex_unlock(&mutex);

      		break;
        }

        pthread_mutex_lock(&mutex);
        clients = (struct tInfo*) realloc(clients, sizeof(struct tInfo) * (clientQuantity + 1));
        clients[clientQuantity].socket = s;
        clients[clientQuantity].address = inet_ntoa(a.sin_addr);
        clients[clientQuantity].port = a.sin_port;
        clients[clientQuantity].number = clientQuantity;
        indexClient = clientQuantity;
        if(pthread_create(&(clients[clientQuantity].threadId), NULL, clientHandler, (void*) &indexClient)) {
            printf("ERROR: Can't create thread for client!\n"); fflush(stdout);
            continue;
        }
        pthread_mutex_unlock(&mutex);
        
        clientQuantity++;
    }

    printf("ENDED LISTENER CONNECTIONS!\n"); fflush(stdout);
}

int readN(int socket, char* buf){
	int result = 0;
	int readedBytes = 0;
	int sizeMsg = SIZE_MSG;
	readedBytes = recv(socket, buf, sizeMsg, 0);
	result += readedBytes;
	sizeMsg -= readedBytes;
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

void* kickClient(int kickNum){
	pthread_mutex_lock(&mutex);
	for(int i = 0; i < clientQuantity; i++){
		if(clients[i].number == kickNum) {
			shutdown(clients[i].socket, 2);
			close(clients[i].socket);
			clients[i].socket = -1;
			break;
		}
	}
	pthread_mutex_unlock(&mutex);
}
