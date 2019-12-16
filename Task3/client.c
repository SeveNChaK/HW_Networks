#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "declaration.h"
#include "dexchange.h"

struct Path {
    int argc;
    char dirs[MAX_QUANTITY_ARGS_CMD][SIZE_ARG];
    char pathLine[SIZE_MSG];
} workDir;

struct Path tempPath;

struct sockaddr_in serverInfo;
char preDir[SIZE_MSG];

void execCommand(const int sock, struct sockaddr_in *serverInfo);
void parsePath(struct Path *pathStruct, const char *path);
void makePathLine(const struct Path pathStruct, char *result);
void loadFile(const int sock, struct sockaddr_in tempServerInfo, char *fileName);
void downloadFile(const int sock, struct sockaddr_in *tempServerInfo, char *fileName);
int isWho(char *path);

int main(int argc, char **argv) {

    if(argc != 3){
        fprintf(stdout,"%s\n%s\n", "Неверное количество аргументов!", "Необходим вызов: ./client [IP] [PORT]");
        exit(1);
    }
    char *ip = (char*) argv[1];
    int port = atoi(argv[2]);

    bzero(&serverInfo, sizeof(serverInfo));
    serverInfo.sin_family = AF_INET;
    if (inet_aton(ip, serverInfo.sin_addr.s_addr) == 0){
        fprintf(stderr, "0\n");
        exit(1);
    }
    serverInfo.sin_port = htons(port);

    fprintf(stdout, "Создаю сокет...\n");

    int sock = -1;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        fprintf(stdout,"Не удалось создать сокет! Приложение не запущено.\n");
        exit(1);
    }

    struct sockaddr_in myInfo;
    bzero(&myInfo, sizeof(myInfo));
    myInfo.sin_family = AF_INET;
    myInfo.sin_port = htons(0);
    myInfo.sin_addr.s_addr = htonl(INADDR_ANY); 

    if (bind(sock, (struct sockaddr_in*) &myInfo, sizeof(myInfo)) < 0) {
        fprintf(stdout,"Не удалось настроить адрес сокета! Приложение не запущено.\n");
        close(sock);
        exit(1);
    }

    bzero(preDir, sizeof(preDir));
    strcpy(preDir, "$>");

    struct sockaddr_in tempServerInfo;
    struct Message msg;
    char inputBuf[SIZE_MSG];
    for (;;) {
        fprintf(stdout, "$");
        fprintf(stdout, "%s", workDir.pathLine);
        fprintf(stdout, ">\n");

        bzero(inputBuf, sizeof(inputBuf));
        fgets(inputBuf, sizeof(inputBuf), stdin);
        inputBuf[strlen(inputBuf) - 1] = '\0';

        // fprintf(stderr, "stdin - %d\n", strlen(inputBuf));

        if (!strcmp("/quit", inputBuf)) {
            break;
        }

        char *sep = " ";
        char *arg = strtok(inputBuf, sep);
    
        if(arg == NULL){
            fprintf(stdout,"Дичь! Попробуйте еще раз.\n");
            continue;
        }

        char tempStr[SIZE_MSG] = { 0 };

        if (strcmp(arg, "/load") == 0) {
            arg = strtok(NULL, sep);

            if (arg == NULL) {
                fprintf(stdout,"Не точное число параметров!\n");
                continue;
            }

            parsePath(&tempPath, arg);

            strcat(tempStr, "/load ");
            strcat(tempStr, workDir.pathLine);
            strcat(tempStr, "/");
            strcat(tempStr, tempPath.dirs[tempPath.argc - 1]);
        } else if (strcmp(arg, "/dload") == 0) {
            arg = strtok(NULL, sep);

            if (arg == NULL) {
                fprintf(stdout,"Не точное число параметров!\n");
                continue;
            }

            parsePath(&tempPath, arg);

            strcat(tempStr, "/dload ");
            strcat(tempStr, workDir.pathLine);
            strcat(tempStr, "/");
            strcat(tempStr, arg);
        } else if (strcmp(arg, "/cd") == 0) {
            arg = strtok(NULL, sep);

            if (arg == NULL) {
                fprintf(stdout,"Не точное число параметров!\n");
                continue;
            }

            memcpy(&tempPath, &workDir, sizeof(workDir));
            if (strcmp(arg, "..") == 0) {
                if (tempPath.argc == 0) {
                    fprintf(stdout,"Вы находитесь в корневой директории.\n");
                    continue;
                }

                tempPath.argc--;

                strcat(tempStr, "/cd ");
                makePathLine(tempPath, tempStr);
                strcat(tempStr, "/");
            } else {
                strcat(tempStr, "/cd ");
                makePathLine(tempPath, tempStr);
                strcat(tempStr, "/");
                strcat(tempStr, arg);
            }
        } else if (strcmp(arg, "/ls") == 0){
            strcat(tempStr, "/ls ");
            strcat(tempStr, workDir.pathLine);
            strcat(tempStr, "/");
        }

        if (safeSendMsg(sock, serverInfo, CODE_CONNECT, "CONNECT", strlen("CONNECT")) < 0){
            fprintf(stdout,"Проблемы с отправкой команды на сервер - 1! Попробуйте еще раз.\n");
            continue;
        }

        if (safeReadMsg(sock, &tempServerInfo, &msg) < 0) {
            fprintf(stdout,"Проблемы с отправкой команды на сервер - 2! Попробуйте еще раз.\n");
            continue;
        }

        if (strlen(tempStr) == 0) {
            if (safeSendMsg(sock, tempServerInfo, CODE_CMD, inputBuf, strlen(inputBuf)) < 0) {
                fprintf(stdout,"Проблемы с отправкой команды на сервер - 3! Попробуйте еще раз.\n");
                continue;
            }
        } else {
            if (safeSendMsg(sock, tempServerInfo, CODE_CMD, tempStr, strlen(tempStr)) < 0) {
                fprintf(stdout,"Проблемы с отправкой команды на сервер - 3! Попробуйте еще раз.\n");
                continue;
            }
        }

        execCommand(sock, &tempServerInfo);
    }

    close(sock);
    fprintf(stdout,"Приложение завершило свою работу. До скорой встречи!\n");

    return 0;
}

void makePathLine(const struct Path pathStruct, char *result) {
    if (pathStruct.argc > 0) {
        strcat(result, "/");
    }
    for (int i = 0; i < pathStruct.argc - 1; i++) {
        strcat(result, pathStruct.dirs[i]);
        strcat(result, "/");
    }
    strcat(result, pathStruct.dirs[pathStruct.argc - 1]);
}

void execCommand(const int sock, struct sockaddr_in *serverInfo) {
    struct Message msg;
    int code = -1;

    int wrongQuantity = 10;
    for (;;) {
        if(safeReadMsg(sock, serverInfo, &msg) < 0) {
            fprintf(stdout,"Проблемы с получением ответа от сервера! Попробуйте еще раз.\n");
            return;
        }

        code = msg.type;

        if (code == CODE_WORK_DIR) {
            parsePath(&workDir, msg.data);
        } else if (code == CODE_ERROR) {
            fprintf(stdout,"Возникла ошибка!\n%s\n", msg.data);
        } else if (code == CODE_INFO) {
            fprintf(stdout,"%s\n", msg.data);
        } else if (code == CODE_LOAD_FILE) {
            loadFile(sock, *serverInfo, tempPath.pathLine);
        } else if (code == CODE_DLOAD_FILE) {
            downloadFile(sock, serverInfo, tempPath.dirs[tempPath.argc - 1]);
            break;
        } else if (code == CODE_OK) {
            break;
        } else {
            fprintf(stdout,"Пришло что-то не то! Попробуйте еще раз.");

            if (--wrongQuantity == 0) {
                fprintf(stdout,"Получено много неверных пакетов. Предположительно что-то не так с сетью. Сообщение не получено.\n");
                break;
            }
        }
    }

    return;
}

void parsePath(struct Path *pathStruct, const char *path) {
    fprintf(stderr, "Path - %s\n", path);
    char sourcePath[SIZE_MSG] = {0};
    memcpy(sourcePath, path, strlen(path));

    if (strcmp(path, "/") == 0) {
        // pathStruct->argc = 0;
        bzero(pathStruct, sizeof(struct Path));
        return;
    }

    int countArg = 0;
    char *sep = "/";
    char *arg = strtok(path, sep);

    char dirs[MAX_QUANTITY_ARGS_CMD][SIZE_ARG];

    fprintf(stdout,"Сари - %s\n", arg);
    
    if(arg == NULL){
        fprintf(stdout,"С путем что-то не так %s.", path);
        return;
    }
  

    while (arg != NULL && countArg <= MAX_QUANTITY_ARGS_CMD) {
        countArg ++;
        strcpy(dirs[countArg - 1], arg);
        arg = strtok(NULL, sep);
        fprintf(stdout,"Сари - %s\n", arg);
    }

    if(countArg > MAX_QUANTITY_ARGS_CMD){
        fprintf(stdout,"Слишком много директорий в пути.\n", path);
        return;
    }

    bzero(pathStruct, sizeof(struct Path));
    pathStruct->argc = countArg;
    memcpy(pathStruct->dirs, dirs, sizeof(dirs));
    fprintf(stderr, "DO %s %s\n", pathStruct->pathLine, workDir.pathLine);
    strcpy(pathStruct->pathLine, sourcePath);
    fprintf(stderr, "POSLE %s %s\n", pathStruct->pathLine, workDir.pathLine);

    fprintf(stderr, "aga %s\n", path);
}

int isWho(char *path) {
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

void loadFile(const int sock, struct sockaddr_in tempServerInfo, char *fileName) {
    if (isWho(fileName) != 1) {
        fprintf(stdout,"%s - это не файл!", fileName);
        safeSendMsg(sock, tempServerInfo, CODE_ERROR, "Отмена.", strlen("Отмена."));
        return;
    }

    FILE *file = fopen(fileName, "rb");
    if(file == NULL){
        fprintf(stdout,"Не удалось загрузить файл - %s\n", fileName);
        safeSendMsg(sock, tempServerInfo, CODE_ERROR, "Отмена.", strlen("Отмена."));
        return;
    }

    char section[SIZE_MSG] = {'\0'};
    int res = 0, err;
    while ((res = fread(section, sizeof(char), sizeof(section), file)) != 0) {
        fprintf(stdout,"res = %d\n", res);
        err = safeSendMsg(sock, tempServerInfo, CODE_FILE, section, res);
        if(err == -1){
            fprintf(stdout,"Не удалось отправить файл - %s\n", fileName);
            fclose(file);
            return;
        }
        bzero(section, sizeof(section));
    }

    fclose(file);

    err = safeSendMsg(sock, tempServerInfo, CODE_OK, "OK", 2);
    if(err == -1){
        fprintf(stdout,"Не удалось отправить подтверждение...");
    }
}

void downloadFile(const int sock, struct sockaddr_in *tempServerInfo, char *fileName) {
    FILE *file = fopen(fileName, "wb");
    if(file == NULL){
        fprintf(stdout,"Не удалось скачать файл - %s\n", fileName);
        safeSendMsg(sock, *tempServerInfo, CODE_ERROR, "Отмена", strlen("Отмена"));
        return;
    }

    struct Message msg;
    int err;
    for(;;){
        err = safeReadMsg(sock, tempServerInfo, &msg);
        if(err == -1){
            fprintf(stdout,"Не удалось принять кусок файла - %s. Данные не были сохранены.\n", fileName);
            fclose(file);
            remove(fileName);
            return;
        }

        if(msg.type == CODE_FILE){
            fwrite(msg.data, sizeof(char), msg.length, file);
        } else if(msg.type == CODE_OK){
            fprintf(stdout,"Файл: %s - скачан.\n", fileName);
            break;
        } else {
            fprintf(stdout,"Не удалось принять кусок файла - %s. Данные не были сохранены.\n", fileName);
            fclose(file);
            remove(fileName);
            return;
        }
    }

    fclose(file);
}