#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int sockfd; /* Дескриптор сокета */
    int n, len; /* Переменные для различных длин и количества символов */
    char sendline[1000], recvline[1000]; /* Массивы для отсылаемой и принятой строки */
    struct sockaddr_in servaddr, cliaddr; /* Структуры дляадресов сервера и клиента */

    /* Сначала проверяем наличие второго аргумента в 
    командной строке. При его отсутствии ругаемся и прекращаем 
    работу */
    if(argc != 2){
        printf("Usage: a.out <IP address>\n");
        exit(1);
    }

    /* Создаем UDP сокет */
    if((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
        perror(NULL); /* Печатаем сообщение об ошибке */
        exit(1);
    }

    /* Заполняем структуру для адреса клиента: семейство 
    протоколов TCP/IP, сетевой интерфейс – любой, номер порта 
    по усмотрению операционной системы. Поскольку в структуре
    содержится дополнительное не нужное нам поле, которое 
    должно быть нулевым, перед заполнением обнуляем ее всю */
    bzero(&cliaddr, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(0);
    cliaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Настраиваем адрес сокета */
    if (bind(sockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0) {
        perror(NULL);
        close(sockfd); /* По окончании работы закрываем дескриптор сокета */
        exit(1);
    }

    /* Заполняем структуру для адреса сервера: 
    семейство протоколов TCP/IP, сетевой интерфейс – из аргумента
    командной строки, номер порта 7. Поскольку в 
    структуре содержится дополнительное не нужное нам
    поле, которое должно быть нулевым, перед заполнением
    обнуляем ее всю */
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(7);
    if (inet_aton(argv[1], &servaddr.sin_addr) == 0) {
        printf("Invalid IP address\n");
        close(sockfd); /* По окончании работы закрываем 
            дескриптор сокета */
        exit(1);
    }

    /* Вводим строку, которую отошлем серверу */
    printf("String => ");
    fgets(sendline, 1000, stdin);

    /* Отсылаем датаграмму */
    if (sendto(sockfd, sendline, strlen(sendline)+1, 0, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
    perror(NULL);
    close(sockfd);
    exit(1);
    }

    /* Ожидаем ответа и читаем его. Максимальная 
    допустимая длина датаграммы – 1000 символов, 
    адрес отправителя нам не нужен */
    if ((n = recvfrom(sockfd, recvline, 1000, 0, (struct sockaddr *) NULL, NULL)) < 0) {
        perror(NULL);
        close(sockfd);
        exit(1);
    }

    /* Печатаем пришедший ответ и закрываем сокет */
    printf("%s\n", recvline);
    close(sockfd);
    return 0;
}