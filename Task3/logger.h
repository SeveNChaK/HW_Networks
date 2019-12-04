#ifndef DECLARATION_H
#define DECLARATION_H

#define LOG_DEBUG_ON 1
#define LOG_INFO_ON 1
#define LOG_ERROR_ON 1

void logDebug(const char *format, ...) {
	if (LOG_DEBUG_ON == 1) {
		fprintf(stdout, format, ...);
	}
}

void logInfo(const char *format, ...) {
	if (LOG_INFO_ON == 1) {
		fprintf(stdout, format, ...);
	}
}

void logError(const char *format, ...) {
	if (LOG_ERROR_ON == 1) {
		fprintf(stderr, format, ...);
	}
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

#endif