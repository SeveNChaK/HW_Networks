#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>

#include "logger.h"
#include "declaration.h"
#include "dexchange.h"

struct Path{
    int argc;
    char dirs[MAX_QUANTITY_ARGS_CMD][SIZE_ARG];
    char pathLine[SIZE_MSG];
} workDir;

struct sockaddr_in serverInfo;
char preDir[SIZE_MSG];

int main(int argc, char **argv) {

    if(argc != 3){
        logError("%s\n%s\n", "Неверное количество аргументов!", "Необходим вызов: ./client [IP] [PORT]");
        exit(1);
    }
    char *ip = (char *) argv[1];
    int port = *((int*) argv[2]);

    bzero(&serverInfo, sizeof(serverInfo));
    serverInfo.sin_family = AF_INET;
    serverInfo.sin_addr.s_addr = inet_addr(ip); 
    serverInfo.sin_port = htons(port);


    int sock = -1;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        logError("Не удалось создать сокет! Приложение не запущено.\n");
        exit(1);
    }

    struct sockaddr_in myInfo;
    bzero(&myInfo, sizeof(myInfo));
    serverInfo.sin_family = AF_INET;
    serverInfo.sin_addr.s_addr = inet_addr(INADDR_ANY); 

    if (bind(sock, (struct sockaddr*) &myInfo, sizeof(myInfo)) < 0) {
        logError("Не удалось настроить адрес сокета! Приложение не запущено.\n");
        close(sock);
        exit(1);
    }

    bzero(preDir, sizeof(preDir));
    strcpy(preDir, "$>");

    struct sockaddr_in tempServerInfo;
    struct Message msg;
    char inputBuf[SIZE_MSG];
    for (;;) {
        fprintf(stdout, "%s", preDir);
        bzero(inputBuf, sizeof(inputBuf));
        fgets(inputBuf, sizeof(inputBuf), stdin);
        inputBuf[strlen(inputBuf) - 1] = '\0';

        if (!strcmp("/quit", inputBuf)) {
            break;
        }

        if (safeSendMsg(sock, serverInfo, CODE_CONNECT, "CONNECT", strlen("CONNECT")) < 0){
            logError("Проблемы с отправкой команды на сервер - 1. Соединение разорвано!\n");
            break;
        }

        if (safeReadMsg(sock, &tempServerInfo, &msg) < 0) {
            logError("Проблемы с отправкой команды на сервер - 2. Соединение разорвано!\n");
            break;
        }

        
//---дичь
        char *sep = " ";
        char *arg = strtok(inputBuf, sep);
    
        if (!strcmp(arg, "load")) {

        } else if (!strcmp(arg, "dload")) {

        }

        if(arg == NULL){
            sprintf(errorString, "Команда: %s - не поддается парсингу.\nВведите корректную команду. Используйте: help\n", msg->data);
            return -1;
        }
//-------------
        
        if (safeSendMsg(sock, &tempServerInfo, CODE_CMD, inputBuf, strlen(inputBuf)) < 0) {
            logError("Проблемы с отправкой команды на сервер - 3. Соединение разорвано!\n");
            break;
        }

        if (execCommand(sock, &tempServerInfo) == 0) {
            logError("Проблемы с сетью! Соединение разорвано.\n");
            break;
        }
    }

    close(sock;)
    logInfo("Приложение завершило свою работу. До скорой встречи!\n");

    return 0;
}

void execCommand(const int sock, struct sockaddr_in *serverInfo) {
    struct Message msg;
    int code = -1;

    if(safeReadMsg(sock, serverInfo, &msg) < 0) {
        logError("Проблемы с получением ответа от сервера. Соединение разорвано!\n");
        return 0;
    }

    code = package.code;

    if (code == CODE_ERROR) {
        logError("Возникла ошибка!\n%s\n", msg.data);
    } else if (code == CODE_INFO) {
        logInfo("%s\n", msg.data);
    } else if (code == CODE_FILE_REQUEST) {
        loadFile(sock, &tempServerInfo, msg.data);
    } else if (code == CODE_FILE_AVAILABLE) {
        downloadFile(sock, &tempServerInfo, msg.data);
    } else if (code == CODE_OK) {
        return;
    } else {
        logError("Пришло что-то не то! Попробуйте еще раз.");
    }

    return;
}

/**
Отправка файла серверу.
Входные значения:
    int sock - соединение с сервером;
    char *fileName - путь к файлу на стороне клиента.
*/
void loadFile(const int sock, struct sockaddr_in tempServerInfo, char *fileName){
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