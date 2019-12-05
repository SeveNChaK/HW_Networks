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
#include "logger.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct Path{
    int count;
    char dirs[MAX_QUANTITY_ARGS_CMD][SIZE_ARG];
};

struct ConnectInfo {
    struct Package package;
    struct sockaddr_in clientInfo;
};

char *rootDir;

void initServerSocket(int *serverSocket, int port);
unsigned short initServerSocketWithRandomPort(int *serverSocket);
void* asyncTask(void* args);
int execClientCommand(const int socket, const struct sockaddr_in *clientInfo, const struct Message *msg, const char *errorString);
int parseCmd(struct Message *msg, const char *errorString);
int validateCommand(const struct Message msg, const char *errorString);
int checkRegEx(const char *str, const char *mask);
int sendListFilesInDir(const int socket, const struct sockaddr_in *clientInfo, const char *path, char *errorString);
int changeClientDir(const int socket, const struct sockaddr_in *clientInfo, const char *path, const char *errorString);
int readFile(const int socket, const struct sockaddr_in *client, const char *fileName, const char *errorString);
int sendFile(const int socket, const struct sockaddr_in *client, const char *fileName, char *errorString);

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
        pthread_join(workers[i], NULL);
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
void initServerSocket(int *serverSocket, int port) {
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
unsigned short initServerSocketWithRandomPort(int *serverSocket) {
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
    unsigned short newPort = initServerSocketWithRandomPort(&tempSocket);

    if (safeSendMsg(tempSocket, connectInfo.clientInfo, CODE_CONNECT, &newPort, sizeof(newPort)) < 0) {
        logError("Запущена задача клиента IP - %s  PORT - %d не выполнена!\n", 
            inet_ntoa(connectInfo.clientInfo.sin_addr), connectInfo.clientInfo.sin_port);
        return;
    }

    logDebug("Соединение должно быть установилось.\n");

    struct sockaddr_in clientInfo; bzero(&clientInfo, sizeof(clientInfo));
    struct Message msg;

    if (safeReadMsg(tempSocket, &clientInfo, &msg) < 0) {
        logError("Не смогли считать команду от клиента в задаче!\n");
        return;
    }

    char errorString[SIZE_PACK_DATA] = { 0 };
    if (execClientCommand(socket, &clientInfo, &msg, errorString) == -1) {
        logDebug("Ошибка обработки команды клиента!\n");
        
        if (safeSendMsg(tempSocket, clientInfo, CODE_ERROR, &errorString, sizeof(errorString)) < 0) {
            logError("Не смогли отправить сообщение об ошибке!\n");
            return;
        }
    } else {
        logDebug("Команда обработана!\n");

        if (safeSendMsg(tempSocket, clientInfo, CODE_OK, "OK", 2) < 0) {
            logError("Не смогли отправить сообщение усешной обработки команды!\n");
            return;
        }
    }
}

int execClientCommand(const int socket, const struct sockaddr_in *clientInfo, const struct Message *msg, const char *errorString) {
    bzero(errorString, sizeof(errorString));
    
    if(parseCmd(msg, errorString) == -1){
        logError("Не удалось распарсить команду клиента: %s\nОписание ошибки: %s\n", msg->data, errorString);
        return -1;
    }
    if(validateCommand(*msg, errorString) == -1){
        logError("Команда клиента: %s - неккоретна!\nОписание ошибки: %s\n", msg->data, errorString);
        return -1;
    }

    logDebug("Команда клиента: %s - корректна.\n", msg->data);
    
    
    if(!strcmp(msg->argv[0], "/ls")) {
        return sendListFilesInDir(socket, clientInfo, msg->argv[1], errorString);
    } else if(!strcmp(msg->argv[0], "/cd")) {
        return changeClientDir(socket, clientInfo, msg->argv[1], errorString);
    } else if(!strcmp(msg->argv[0], "/load")) {
        return readFile(socket, clientInfo, msg->argv[1], errorString);
    } else if(!strcmp(msg->argv[0], "/dload")){
        return sendFile(socket, clientInfo, msg->argv[1], errorString);
    } else {
        logError("Хоть мы все и проверили, но что-то с ней не так: %s\n", msg->data);
        return -1;
    }

    return 1;
}

int parseCmd(struct Message *msg, const char *errorString) {
    bzero(errorString, sizeof(errorString));

    int countArg = 0;
    char *sep = " ";
    char *arg = strtok(msg->data, sep);

    logDebug("Сари - %s\n", arg);
    
    if(arg == NULL){
        sprintf(errorString, "Команда: %s - не поддается парсингу.\nВведите корректную команду. Используйте: help\n", msg->data);
        return -1;
    }

    while(arg != NULL && countArg <= MAX_QUANTITY_ARGS_CMD){
        countArg ++;
        strcpy(msg->argv[countArg - 1], arg);
        arg = strtok(NULL, sep);
    }

    if(countArg > MAX_QUANTITY_ARGS_CMD){
        sprintf(errorString, "Слишком много аргументов в команде: %s - таких команд у нас нет. Используйте: help\n", msg->data);
        return -1;
    }

    msg->argc = countArg;
    
    return 1;
}

int validateCommand(const struct Message msg, const char *errorString) {
    bzero(errorString, sizeof(errorString));

    int argc = msg.argc;
    char *firstArg = msg.argv[0];
    char *cmdLine = msg.data;

    if (!strcmp(firstArg, "/ls")) {
        if (argc != 2) {
            sprintf(errorString, "Команда: %s - имеет лишние аргументы. Воспользуейтесь командой: help\n", msg.data);
            return -1;
        }
    } else if (!strcmp(firstArg, "/cd")) {
        if (argc != 2) {
            sprintf(errorString, "Команда: %s - имеет много или мало аргументов. Воспользуейтесь командой: help\n", msg.data);
            return -1;
        }
    } else if (!strcmp(firstArg, "/load")) {
        if (argc != 2) {
            sprintf(errorString, "Команда: %s - имеет много или мало аргументов. Воспользуейтесь командой: help\n", msg.data);
            return -1;
        }
    } else if (!strcmp(firstArg, "/dload")) {
        if (argc != 2) {
            sprintf(errorString, "Команда: %s - имеет много или мало аргументов. Воспользуейтесь командой: help\n", msg.data);
            return -1;
        }
    } else if (!strcmp(firstArg, "/kick")) {
        if (argc != 2) {
            sprintf(errorString, "Команда: %s - имеет много или мало аргументов. Воспользуейтесь командой: help\n", msg.data);
            return -1;
        }

        int err = checkRegEx(msg.argv[1], "^[0-9]+$");
        if (err == -1) {
            sprintf(errorString, "С командой: %s - что-то не так. Воспользуейтесь командой: /help\n", msg.data);
            return -1;
        } else if (err == -2) {
            sprintf(errorString, "Команда: %s - имеет некорретные аргументы. Воспользуейтесь командой: /help\n", msg.data);
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

int checkRegEx(const char *str, const char *mask) {
    regex_t preg;
    int err = regcomp (&preg, mask, REG_EXTENDED);
    if(err != 0){
        logError("Не получилось скомпилировать регулярное выражение: %s\n", mask);
        return -1;
    }
    regmatch_t pm;
    err = regexec (&preg, str, 0, &pm, 0);
    if(err != 0){
        return -2;
    }
    return 0;
}

int sendListFilesInDir(const int socket, const struct sockaddr_in *clientInfo, const char *path, char *errorString) {
    bzero(errorString,sizeof(errorString));

    DIR *dir = opendir(path);
    if (dir == NULL) {
        logDebug("Не смог открыть директорию - %s\n", path);
        sprintf(errorString, "Не удалось получить список файлов из директории - %s. Возможно она не существует.\n", path);
        return -1;
    }

    struct dirent *dirent;
    while ((dirent = readdir(dir)) != NULL) {
        if (dirent->d_name[0] == '.') {
            continue;
        }
        
        if (safeSendMsg(socket, *clientInfo, CODE_INFO, dirent->d_name, strlen(dirent->d_name)) == -1) {
            logDebug("Проблема с отправкой имени файла из директории - %s\n", path);
            sprintf(errorString, "Не получилось отправить навзание файла из директории - %s", path);
            return -1;
        }
    }

    if (closedir(path) == -1) {
        logDebug("Беда! Не могу закрыть директорию - %s\n", path);
        sprintf(errorString, "Проблемы с директорией! - %s", path);
        return -1;
    }

    return 1;
}

int changeClientDir(const int socket, const struct sockaddr_in *clientInfo, const char *path, const char *errorString) {

    logDebug("Целевая директория - %s\n", path);

    if(isWho(path) != 2){
        sprintf(errorString, "%s - это не каталог! Или такого каталога не существует.", path);
        return -1;
    }

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

int readFile(const int socket, const struct sockaddr_in *clientInfo, const char *fileName, const char *errorString) {

    FILE *file = fopen(fileName, "wb");
    if (file == NULL) {
        logError("Не удалось открыть файл: %s - для записи!\n", fileName);
        sprintf(errorString, "Не удалось загрузить файл - %s.", fileName);
        return -1;
    }

    int err;
    struct Message msg;
    for(;;){
        err = safeReadMsg(socket, clientInfo, &msg);
        if(err == -1){
            logError("Не удалось принять кусок файла - %s. Данные не были сохранены.\n", fileName);
            sprintf(errorString, "Не удалось загрузить файл - %s.", fileName);
            fclose(file);
            remove(fileName);
            return -1;
        }

        if (msg.type == CODE_FILE) {
            logDebug("Пишу байт - %d\n", msg.length);
            fwrite(msg.data, sizeof(char), msg.length, file);
        } else {
            logDebug("Пришло неправильное сообщение с кодом - %d.\n", msg.type);
            sprintf(errorString, "Не удалось загрузить файл - %s.", fileName);
            fclose(file);
            remove(fileName);
            return -1;
        }
    }
    fclose(file);
    
    return 1;
}

int sendFile(const int socket, const struct sockaddr_in *clientInfo, const char *fileName, char *errorString) {

    if (isWho(fileName) != 1) {
        sprintf(errorString, "%s - это не файл!", fileName);
        return -1;
    }

    //КАЖИСЬ НАДО ЕЩЕ ОТПРАВЛЯТЬ ЗАПРОС НА ТО ЧТ ОЯ ХОЧУ ОТПРАВИТЬ

    FILE *file = fopen(fileName, "rb");
    if (file == NULL) {
        logError("Не удалось открыть файл: %s - для записи!\n", fileName);
        sprintf(errorString, "Не удалось отправить файл - %s.", fileName);
        return -1;
    }

    char section[SIZE_MSG] = {'\0'};
    int res = 0, err;
    while ((res = fread(section, sizeof(char), sizeof(section), file)) != 0) {
        err = safeSendMsg(socket, *clientInfo, CODE_FILE, section, res);
        if (err == -1) {
            logError("Не удалось отправить кусок файла - %s\n", fileName);
            sprintf(errorString, "Не удалось отправить файл - %s\n", fileName);
            fclose(file);
            return -1;
        }
        bzero(section, sizeof(section));
    }

    fclose(file);
    
    return 1;
}