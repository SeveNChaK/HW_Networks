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
    strcpy(preDir, "~$");

    char inputBuf[SIZE_MSG];
    for (;;) {
        fprintf(stdout, "%s", preDir);
        bzero(inputBuf, sizeof(inputBuf));
        fgets(inputBuf, sizeof(inputBuf), stdin);
        inputBuf[strlen(inputBuf) - 1] = '\0';

        if (!strcmp("/quit", inputBuf)) {
            break;
        }

        if (safeSendMsg(sock, serverInfo, CODE_CONNECT, "CONNECT", strlen("CONNECT")) == -1){
            logError("Проблемы с отправкой команды на сервер. Соединение разорвано!\n");
            break;
        }

        if (execCommand(sock) == 0) {
            break;
        }
    }

    close(sock;)
    logInfo("Приложение завершило свою работу. До скорой встречи!");

    return 0;
}