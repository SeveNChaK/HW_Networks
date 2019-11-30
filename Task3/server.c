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
#include "log.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct Command {
    int argc;
    char argv[MAX_QUANTITY_ARGS_CMD][SIZE_ARG];
    char sourceCmdLine[SIZE_CMD];
};

struct Path{
    int count;
    char dirs[MAX_QUANTITY_ARGS_CMD][SIZE_ARG];
};

struct Client {
    char address[SIZE_MY_STR];
    int port;
    struct Path dir;
    struct Package lastPackage;
} *clients;
int clientQuantity = 0;
char *rootDir;

int main( int argc, char** argv) {

    if(argc != 3){
        logError("%s\n%s\n", "Неверное количество аргументов!", "Необходим вызов: ./server [PORT] [WORK_PATH]");
        exit(1);
    }

    int port = *((int*) argv[1]);
    rootDir = argv[2];
    int serverSocket = -1;
    initServerSocket(&serverSocket, port);

    logInfo("%s\n", "Сервер завершил работу.");    
    return 0;
}

/**
Инициализация UDP сокета. Если произойдет какая-то ошибка, то программа будет
завершена вызовом функции exit().
Входные значения:
    int *serverSocket - ссылка на переменную сокета сервера;
    int port - порт, на котором сервер будет ожидать пакеты.
*/
void initServerSocket(int *serverSocket, int port){
    struct sockaddr_in servaddr;
    logInfo("Инициализация сервера...");

    /* Заполняем структуру для адреса сервера: семейство
    протоколов TCP/IP, сетевой интерфейс – любой, номер порта - 
    port. Поскольку в структуре содержится дополнительное не
    нужное нам поле, которое должно быть нулевым, перед 
    заполнением обнуляем ее всю */
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    *serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (*serverSocket < 0) {
        logError(stderr, "%s\n", "Не удалось создать сокет!");
        exit(1);
    }

    // int enable = 1;
    // if (setsockopt(serverSocket, IPPROTO_TCP, SO_REUSEADDR, &enable, sizeof(int)) < 0){
    //     fprintf(stderr, "%s\n", "setsockopt(SO_REUSEADDR) failed!");
    // }

    int resBind = bind(*serverSocket, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (resBind < 0 ) {
        logError("%s\n", "Не удалось выполнить присваивание имени сокету!");
        close(serverSocket);
        exit(1);
    }

    logInfo("%s\n", "Инициализация сервера прошла успешно.");
}


int main() {
    int sockfd; /* Дескриптор сокета */
    int clilen, n; /* Переменные для различных длин и количества символов */
    char line[1000]; /* Массив для принятой и отсылаемой строки */
    struct sockaddr_in servaddr, cliaddr; /* Структуры для адресов сервера и клиента */

    /* Заполняем структуру для адреса сервера: семейство
    протоколов TCP/IP, сетевой интерфейс – любой, номер порта 
    51000. Поскольку в структуре содержится дополнительное не
    нужное нам поле, которое должно быть нулевым, перед 
    заполнением обнуляем ее всю */
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(51000);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Создаем UDP сокет */
    if ((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror(NULL); /* Печатаем сообщение об ошибке */
        exit(1);
    }

    /* Настраиваем адрес сокета */
    if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror(NULL);
        close(sockfd);
        exit(1);
    }

    while (1) {
        /* Основной цикл обслуживания*/
        /* В переменную clilen заносим максимальную длину
        для ожидаемого адреса клиента */
        clilen = sizeof(cliaddr);
        /* Ожидаем прихода запроса от клиента и читаем его. 
        Максимальная допустимая длина датаграммы – 999 
        символов, адрес отправителя помещаем в структуру 
        cliaddr, его реальная длина будет занесена в 
        переменную clilen */
        if ((n = recvfrom(sockfd, line, 999, 0, (struct sockaddr *) &cliaddr, &clilen)) < 0) {
            perror(NULL);
            close(sockfd);
            exit(1);
        }
        /* Печатаем принятый текст на экране */
        printf("%s\n", line);
        /* Принятый текст отправляем обратно по адресу 
        отправителя */
        if(sendto(sockfd, line, strlen(line), 0, 
        (struct sockaddr *) &cliaddr, clilen) < 0){
            perror(NULL);
            close(sockfd);
            exit(1);
        } /* Уходим ожидать новую датаграмму*/
    }

    return 0;
}