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

struct Path {
    int argc;
    char dirs[MAX_QUANTITY_ARGS_CMD][SIZE_ARG];
    char pathLine[SIZE_MSG];
} workDir;

strcut Path tempPath;

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
            logError("Проблемы с отправкой команды на сервер - 1! Попробуйте еще раз.\n");
            continue;
        }

        if (safeReadMsg(sock, &tempServerInfo, &msg) < 0) {
            logError("Проблемы с отправкой команды на сервер - 2! Попробуйте еще раз.\n");
            continue;
        }

        char *sep = " ";
        char *arg = strtok(inputBuf, sep);
    
        if(arg == NULL){
            logError("Дичь! Попробуйте еще раз.\n");
            continue;
        }

        if (strcmp(arg, "/load") == 0) {
            arg = strtok(NULL, sep);
            parsePath(&tempPath, arg);

            char tempStr[SIZE_MSG] = { 0 };
            strcat(tempStr, "/load ");
            strcat(tempStr, workDir.pathLine);
            strcat(tempStr, "/");
            strcat(tempStr, tempPath.argv[tempPath.argc - 1]);

            if (safeSendMsg(sock, &tempServerInfo, CODE_CMD, tempStr, strlen(tempStr)) < 0) {
                logError("Проблемы с отправкой команды на сервер - 3! Попробуйте еще раз.\n");
                continue;
            }
        } else if (strcmp(arg, "/dload") == 0) {
            arg = strtok(NULL, sep);
            char tempStr[SIZE_MSG] = { 0 };
            strcat(tempStr, "/dload ");
            strcat(tempStr, workDir.pathLine);
            strcat(tempStr, arg);

            if (safeSendMsg(sock, &tempServerInfo, CODE_CMD, tempStr, strlen(tempStr)) < 0) {
                logError("Проблемы с отправкой команды на сервер - 3! Попробуйте еще раз.\n");
                continue;
            }
        } else if (strcmp(arg, "/cd") == 0) {
            arg = strtok(NULL, sep);
            char tempStr[SIZE_MSG] = { 0 };

            memcpy(tempPath, workDir, sizeof(workDir));
            if (tempPath.argc == 0) {
                logInfo("Вы находитесь в корневой директории.");
                continue;
            }

            tempPath.argc--;

            strcat(tempStr, "/cd ");
            makePathLine(tempPath, tempStr);

            if (safeSendMsg(sock, &tempServerInfo, CODE_CMD, tempStr, strlen(tempStr)) < 0) {
                logError("Проблемы с отправкой команды на сервер - 3! Попробуйте еще раз.\n");
                continue;
            }
        } else {
            if (safeSendMsg(sock, &tempServerInfo, CODE_CMD, inputBuf, strlen(inputBuf)) < 0) {
                logError("Проблемы с отправкой команды на сервер - 3! Попробуйте еще раз.\n");
                continue;
            }
        }

        execCommand(sock, &tempServerInfo);
    }

    close(sock;)
    logInfo("Приложение завершило свою работу. До скорой встречи!\n");

    return 0;
}

void execCommand(const int sock, struct sockaddr_in *serverInfo, const struct Path path) {
    struct Message msg;
    int code = -1;

    for (;;) {
        if(safeReadMsg(sock, serverInfo, &msg) < 0) {
            logError("Проблемы с получением ответа от сервера! Попробуйте еще раз.\n");
            return;
        }

        code = msg.code;

        if (code == CODE_WORK_DIR) {
            parsePath(&workDir, msg.data);
        } else if (code == CODE_ERROR) {
            logError("Возникла ошибка!\n%s\n", msg.data);
        } else if (code == CODE_INFO) {
            logInfo("%s\n", msg.data);
        } else if (code == CODE_LOAD_FILE) {
            loadFile(sock, &tempServerInfo, tempPath.pathLine);
        } else if (code == CODE_DLOAD_FILE) {
            downloadFile(sock, &tempServerInfo, path.argv[path.argc - 1]);
            break;
        } else if (code == CODE_OK) {
            break;
        } else {
            logError("Пришло что-то не то! Попробуйте еще раз.");
        }
    }

    return;
}

void parsePath(struct Path *pathStruct, const char *path) {
    int countArg = 0;
    char *sep = "/";
    char *arg = strtok(path, sep);

    char dirs[MAX_QUANTITY_ARGS_CMD][SIZE_ARG];

    logDebug("Сари - %s\n", arg);
    
    if(arg == NULL){
        logError("С путем что-то не так %s.", path);
        return;
  

    while (arg != NULL && countArg <= MAX_QUANTITY_ARGS_CMD) {
        countArg ++;
        strcpy(dirs[countArg - 1], arg);
        arg = strtok(NULL, sep);
        logDebug("Сари - %s\n", arg);
    }

    if(countArg > MAX_QUANTITY_ARGS_CMD){
        logError("Слишком много директорий в пути.\n", path);
        return;
    }

    bzero(pathStruct, sizeof(pathStruct));
    pathStruct->argc = countArg;
    memcpy(pathStruct->argv, dirs, sizeof(dirs));
    strcpy(pathStruct->pathLine, path);
}

void makePathLine(const struct Path pathStruct, char *result) {
    bzero(result, sizeof(result));
    strcat(result, "/");
    for (int i = 0; i < pathStruct.argc - 1; i++) {
        strcat(result, pathStruct.argv[i]);
        strcat(result, "/");
    }
    strcat(result, pathStruct.argv[pathStruct.argc - 1]);
}

/**
Отправка файла серверу.
Входные значения:
    int sock - соединение с сервером;
    char *fileName - путь к файлу на стороне клиента.
*/
void loadFile(const int sock, struct sockaddr_in tempServerInfo, char *fileName){
    if (isWho(fileName) != 1) {
        logError("%s - это не файл!", fileName);
        safeSendMsg(sock, tempServerInfo, CODE_ERROR, "Отмена.", strlen("Отмена."));
        return;
    }

    FILE *file = fopen(fileName, "rb");
    if(file == NULL){
        logError("Не удалось загрузить файл - %s\n", fileName);
        safeSendMsg(sock, tempServerInfo, CODE_ERROR, "Отмена.", strlen("Отмена."));
        return;
    }

    char section[SIZE_MSG] = {'\0'};
    int res = 0, err;
    while ((res = fread(section, sizeof(char), sizeof(section), file)) != 0) {
        logDebug("res = %d\n", res);
        err = safeSendMsg(sock, tempServerInfo, CODE_FILE, section, res);
        if(err == -1){
            logError("Не удалось отправить файл - %s\n", fileName);
            fclose(file);
            return;
        }
        bzero(section, sizeof(section));
    }

    fclose(file);

    err = safeSendMsg(sock, tempServerInfo, CODE_OK, "OK", 2);
    if(err == -1){
        logError("Не удалось отправить подтверждение...");
    }
}

void downloadFile(const int sock, struct sockaddr_in *tempServerInfo, char *fileName) {
    FILE *file = fopen(fileName, "wb");
    if(file == NULL){
        logError("Не удалось скачать файл - %s\n", fileName);
        safeSendMsg(sock, *tempServerInfo, CODE_ERROR, "Отмена", strlen("Отмена"));
        return;
    }

    struct Message msg;
    int err;
    for(;;){
        err = safeReadMsg(sock, tempServerInfo, &msg);
        if(err == -1){
            logError("Не удалось принять кусок файла - %s. Данные не были сохранены.\n", fileName);
            fclose(file);
            remove(fileName);
            return;
        }

        if(package.code == CODE_FILE){
            fwrite(msg.data, sizeof(char), msg.sizeData, file);
        } else if(package.code == CODE_OK){
            logInfo("Файл: %s - скачан.\n", fileName);
            break;
        } else {
            logError("Не удалось принять кусок файла - %s. Данные не были сохранены.\n", fileName);
            fclose(file);
            remove(fileName);
            return;
        }
    }

    fclose(file);
}