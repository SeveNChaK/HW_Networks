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
    int type;
    int argc;
    char argv[MAX_QUANTITY_ARGS_CMD][SIZE_ARG];
    int lengthCmd;
    char sourceCmdLine[SIZE_CMD];
};

struct Path{
    int count;
    char dirs[MAX_QUANTITY_ARGS_CMD][SIZE_ARG];
};

struct ConnectInfo {
    struct Package package;
    struct sockaddr_in clientInfo;
};

// struct Client {
//     char address[SIZE_MY_STR];
//     int port;
//     struct sockaddr_in clientInfo;
//     struct Package lastPackage;
// };
char *rootDir;


int main(int argc, char** argv) {

    if(argc != 3){
        logError("%s\n%s\n", "Неверное количество аргументов!", "Необходим вызов: ./server [PORT] [WORK_PATH]");
        exit(1);
    }

    int port = *((int*) argv[1]);
    rootDir = argv[2];
    int serverSocket = -1;
    initServerSocket(&serverSocket, port);

    pthread_t *workers;
    int quantityWorkers = 0;

    struct ConnectInfo connectInfo;
    while (1) { //ПРИДУМАТЬ КАК ЗАВЕРШАТЬ РАБОТУ СЕРВЕРА (ГЛОБАЛЬНЫЙ ФЛАГ?)
        // struct Client newClient;


        if (readPack(serverSocket, &(connectInfo.clientInfo), &(connectInfo.package)) < 0) { //тут мы получается ждем команду
            logError("Проблемы с прослушиванием серверного сокета. Необходимо перезапустить сервер!\n");
            close(serverSocket);
            exit(1);
        }

        if (connectInfo.package.id != 1 && connectInfo.package.code != CODE_CONNECT) {
            logDebug("Получили какой-то не тот пакет в основном потоке. Нам нужен пакет с id = 1.\n");
            continue;
        }

        /*
        Каждый раз при получении команды создается новый поток для ее обработки. После обработки этой
        команды поток завершается.
        */
        //СДЕЛАТЬ ПОИСК ВОРКЕРОВ!!!!ОБЯЗАТЕЛЬНО!!!!
        //И ТУТ ОПАСНОЕ МЕСТО С ПАМЯТЬЮ МОЖЕТ НЕ УСПЕТЬ СОЗДАТЬСЯ ПОТОК И УЖЕ ПОМЕНЯТЬСЯ ПАМЯТЬ
        workers = (pthread_t*) realloc(workers, sizeof(pthread_t) * (quantityWorkers + 1));
        if(pthread_create(&(workers[quantityWorkers]), NULL, asyncTask, (void*) &connectInfo)) {
            logError("%s\n", "Не удалось создать поток для обработки задачи!");
            continue;
        }
        quantityWorkers++;
    }

    logInfo("Ожидаем завершения работы воркеров.\n");
    for (int i = 0; i < quantityWorkers; i++) {
        pthread_join(workers[i]);
    }

    close(serverSocket);
    free(workers);

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
    logInfo("Инициализация сервера...\n");

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

/*
Создает сокет на рандомном порту.
*/
unsigned short initServerSocketWithRandomPort(int *serverSocket){
    struct sockaddr_in servaddr;
    logInfo("Инициализация сокета на рандомном порту...\n");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    *serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (*serverSocket < 0) {
        logError(stderr, "%s\n", "Не удалось создать сокет на рандомном порту!");
        exit(1);
    }

    int resBind = bind(*serverSocket, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (resBind < 0 ) {
        logError("%s\n", "Не удалось выполнить присваивание имени сокету на рандомном порту!");
        close(serverSocket);
        exit(1);
    }

    logInfo("%s\n", "Инициализация сокета на рандомном порту прошла успешно.");

    return servaddr.sin_port;
}

/**
Функция обрабокти клиента. Вызывается в новом потоке.
Входные значения:
    void* args - аргумент переданный в функцию при создании потока.
        В данном случае сюда приходит int *indexClient - номер клиента.
Возвращаемое значение:
    void* - так надо, чтоб вызывать функцию при создании потока.
*/
void* asyncTask(void* args) {
    struct ConnectInfo connectInfo = *((struct ConnectInfo*) args);

    logDebug("Запущена задача клиента IP - %s  PORT - %d.\n", 
        inet_ntoa(connectInfo.clientInfo.sin_addr), connectInfo.clientInfo.sin_port);

    int tempSocket = -1;
    unsigned short newPort = initServerSocket(&tempSocket);

    if (safeSendMsg(tempSocket, connectInfo.clientInfo, CODE_CONNECT, &newPort, sizeof(newPort)) < 0) {
        logError("Запущена задача клиента IP - %s  PORT - %d не выполнена!\n", 
        inet_ntoa(connectInfo.clientInfo.sin_addr), connectInfo.clientInfo.sin_port);
        return;
    }

    logDebug("Соединение должно быть установилось.\n");

    struct sockaddr_in clientInfo; bzero(&clientInfo, sizeof(*clientInfo));
    struct Command cmd;

    if (safeReadMsg(tempSocket, &clientInfo, &cmd) < 0) {
        logError("Не смогли считать команду от клиента в задаче!\n");
        return;
    }
}