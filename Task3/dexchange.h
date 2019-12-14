#ifndef DEXCHANGE_H
#define DEXCHANGE_H

#define QUANTITY_TRY 10

int safeSourceReadMsg(const int socket, struct sockaddr_in *clientInfo, struct Message *msg) {
	bzero(clientInfo, sizeof(struct sockaddr_in));
	bzero(msg, sizeof(struct Message));

	struct Package package;

	int nextId = 1;
	int expectedMaxId = 0;
	int expectedCode = 0;
	int wrongPack = QUANTITY_TRY;
	for (;;) { //Ждем первый пакет

		if (readPack(socket, clientInfo, &package) < 0) {
            fprintf(stdout,"Ошибка при надежном чтении сообщения!\n");
            return -1;
        }

        if (package.id == nextId) {
        	expectedMaxId = package.maxId;
        	expectedCode = package.code;
        	break;
        } else {
        	if (--wrongPack == 0) {
        		fprintf(stdout,"Получено много неверных пакетов при ожидании первого. Предположительно что-то не так с сетью. Сообщение не получено.\n");
        		return -1;
        	}
        }
	}

	wrongPack = QUANTITY_TRY;
	char *startIndex = &(msg->data[0]);
	for(;;) {

        if (package.maxId == expectedMaxId 
        	&& package.code == expectedCode 
        	&& package.id == nextId
        ) {
        	msg->type = package.code;
        	msg->length += package.lengthData;
        	memcpy(startIndex, package.data, package.lengthData);
        	startIndex = startIndex + package.lengthData;
        	// strcat(msg->data, package.data);

        	fprintf(stderr, "SIZE - %d\n", package.lengthData);

        	package.ack = ACK;
        	if (sendPack(socket, *clientInfo, package) < 0) {
        		fprintf(stdout,"Ошибка отправки подтверждения!\n");
           		return -1;
        	}

        	if (package.id == expectedMaxId) {
        		break;
        	}

        	nextId++;
        } else {
        	fprintf(stdout,"Получен не верный пакет!\n");

        	if (--wrongPack == 0) {
        		fprintf(stdout,"Получено много неверных пакетов. Предположительно что-то не так с сетью. Сообщение не получено.\n");
        		return -1;
        	}

        	package.ack = ACK;
        	if (sendPack(socket, *clientInfo, package) < 0) {
        		fprintf(stdout,"Ошибка отправки подтверждения!\n");
           		return -1;
        	}
        }

        if (readPack(socket, clientInfo, &package) < 0) {
            fprintf(stdout,"Ошибка при надежном чтении сообщения!\n");
            return -1;
        }

    }

    return 1;
}

int safeReadMsg(const int socket, struct sockaddr_in *clientInfo, struct Message *msg) {
	bzero(clientInfo, sizeof(struct sockaddr_in));
	bzero(msg, sizeof(struct Message));

	struct Package package;


	int result;
 	fd_set inputs;
 	struct timeval timeout;
 	FD_ZERO(&inputs);
 	FD_SET(socket, &inputs);

  	timeout.tv_sec = 10;
  	timeout.tv_usec = 0; //микросекунды

	int nextId = 1;
	int expectedMaxId = 0;
	int expectedCode = 0;
	int wrongPack = QUANTITY_TRY;
	for (;;) { //Ждем первый пакет

		//СДЕЛАТЬ СЕЛЕКТОМ ТАЙМ-АУТ ПО ВРЕМНИ
		result = select(FD_SETSIZE, &inputs, NULL, NULL, &timeout);
		if (result == 0) {
			fprintf(stderr, "Долго не получаем ответа! Давай заново\n");
			return -1;
		}

		if (readPack(socket, clientInfo, &package) < 0) {
            fprintf(stdout,"Ошибка при надежном чтении сообщения!\n");
            return -1;
        }

        if (package.id == nextId) {
        	expectedMaxId = package.maxId;
        	expectedCode = package.code;
        	break;
        } else {
        	if (--wrongPack == 0) {
        		fprintf(stdout,"Получено много неверных пакетов при ожидании первого. Предположительно что-то не так с сетью. Сообщение не получено.\n");
        		return -1;
        	}
        }
	}

	wrongPack = QUANTITY_TRY;
	char *startIndex = &(msg->data[0]);
	for(;;) {

        if (package.maxId == expectedMaxId 
        	&& package.code == expectedCode 
        	&& package.id == nextId
        ) {
        	msg->type = package.code;
        	msg->length += package.lengthData;
        	memcpy(startIndex, package.data, package.lengthData);
        	startIndex = startIndex + package.lengthData;
        	// strcat(msg->data, package.data);

        	fprintf(stderr, "SIZE - %d\n", package.lengthData);

        	package.ack = ACK;
        	if (sendPack(socket, *clientInfo, package) < 0) {
        		fprintf(stdout,"Ошибка отправки подтверждения!\n");
           		return -1;
        	}

        	if (package.id == expectedMaxId) {
        		break;
        	}

        	nextId++;
        } else {
        	fprintf(stdout,"Получен не верный пакет!\n");

        	if (--wrongPack == 0) {
        		fprintf(stdout,"Получено много неверных пакетов. Предположительно что-то не так с сетью. Сообщение не получено.\n");
        		return -1;
        	}

        	package.ack = ACK;
        	if (sendPack(socket, *clientInfo, package) < 0) {
        		fprintf(stdout,"Ошибка отправки подтверждения!\n");
           		return -1;
        	}
        }

        result = select(FD_SETSIZE, &inputs, NULL, NULL, &timeout);
        if (result == 0) {
			fprintf(stderr, "Долго не получаем ответа! Давай заново\n");
			return -1;
		}
        //СДЕЛАТЬ СЕЛЕКТОМ ТАЙМ-АУТ ПО ВРЕМНИ
        if (readPack(socket, clientInfo, &package) < 0) {
            fprintf(stdout,"Ошибка при надежном чтении сообщения!\n");
            return -1;
        }

    }

    return 1;
}

int cmpPack(const struct Package pack1, const struct Package pack2) {
	if (pack1.id == pack2.id
		&& pack1.maxId == pack2.maxId
		&& pack1.code == pack2.code
		&& pack1.lengthData == pack2.lengthData
		&& !memcmp(pack1.data, pack2.data, SIZE_PACK_DATA)
	) {
		return 0;
	}

	return -1;
}

int safeSendMsg(const int socket,
				struct sockaddr_in clientInfo,
				const int code,
				char *msg,
				const int lengthMsg
) {
	int tempQuantityPacks = lengthMsg / SIZE_PACK_DATA;
	int lengthLastPack = lengthMsg % SIZE_PACK_DATA;
	int quantityPacks = tempQuantityPacks;
	if (lengthLastPack != 0 || tempQuantityPacks == 0) {
		quantityPacks += 1;
	}

	int result;
 	fd_set inputs;
 	struct timeval timeout;
 	FD_ZERO(&inputs);
 	FD_SET(socket, &inputs);

  	timeout.tv_sec = 10;
  	timeout.tv_usec = 0; //микросекунды

	int sendedPacks = 0;
	struct Package currentPackage;
	struct Package lastPackage;
	struct sockaddr_in fromClientInfo;
	bzero(&currentPackage, sizeof(struct Package));
	int quantityTry = QUANTITY_TRY;
	for (;;) {
		bzero(&lastPackage, sizeof(struct Package));

		if (currentPackage.id != sendedPacks + 1) { //Проверка на то, что отправляем - старый или новый пакет
			bzero(&currentPackage, sizeof(struct Package));
			currentPackage.ack = NO_ACK;
			currentPackage.id = sendedPacks + 1;
			currentPackage.maxId = quantityPacks;
			currentPackage.code = code;
			if (currentPackage.id != quantityPacks) {
				currentPackage.lengthData = SIZE_PACK_DATA;
				memcpy(currentPackage.data, msg, SIZE_PACK_DATA);
				fprintf(stderr, "R1 - %c\n", msg[0]);
				msg = msg + SIZE_PACK_DATA;
				fprintf(stderr, "R2 - %c\n", msg[0]);
			} else {
				currentPackage.lengthData = lengthLastPack;
				memcpy(currentPackage.data, msg, lengthLastPack);
				fprintf(stderr, "R11 - %c\n", msg[0]);
				msg = msg + lengthLastPack;
				fprintf(stderr, "R22 - %c\n", msg[0]);
			}
			fprintf(stderr, "MSG :%s\n", currentPackage.data);
		}

		if (sendPack(socket, clientInfo, currentPackage) < 0) {
			fprintf(stdout,"Ошибка отправки пакета при надежной отправки сообщения!\t%d\n", ntohs(clientInfo.sin_port));
			return -1;
		}

		result = select(FD_SETSIZE, &inputs, NULL, NULL, &timeout);
        if (result == 0) {
			fprintf(stderr, "Долго не получаем ответа! Давай заново\n");
			return -1;
		}

		//СДЕЛАТЬ СЕЛЕКТОМ ТАЙМ-АУТ ПО ВРЕМНИ
		if (readPack(socket, &fromClientInfo, &lastPackage) < 0) {
			fprintf(stdout,"Ошибка при чтении подтверждения!\n");
			return -1;
		}

		if (cmpPack(currentPackage, lastPackage) == 0 && lastPackage.ack == ACK) {
			fprintf(stdout,"Получено подтверждение.\n");
			fprintf(stderr, "%d %d\n", sendedPacks, quantityPacks);
			sendedPacks++;
			fprintf(stderr, "%d %d\n", sendedPacks, quantityPacks);
			if (sendedPacks >= quantityPacks) {
				fprintf(stdout,"Сообщение полностью отправлено, и полученно клиентом.\n");
				break;
			}
		} else if (currentPackage.id == 1) {
			fprintf(stdout,"Начало приходить уже новое сообщение, значит клиент получил предыдущее сообщение.\n");
			fprintf(stdout,"Пропустим его, а потом клиент еще раз пошлет =)\n");
			break;
		} else {
			fprintf(stdout,"Подтверждение не получено, повторная отправка пакета.\n");

			if (--quantityTry == 0) {
				fprintf(stdout,"Слишком много попыток без подтверждения. Возможно проблемы с сетью. Сообщение не отправлено.\n");
				return -1;				
			}
		}
	}

	fprintf(stderr, "%s\n", "Отправка закончена!");

	return 1;
}

/**
Функция читает команду из сокета-дескриптора. Поток будет заблокирован 
на этой функции, пока не будет считана команда или пока не произойдет
закрытие сокета.
Входные значения:
	int socket - сокет-дескриптор, из которого читается сообщение;
	struct Package - ссылка на структуру, которая будет заполнена при чтении.
Возвращаемое значение:
	Количество считанных байт или -1 если не удалось считать команду.
*/
int readPack(const int socket, struct sockaddr_in *clientInfo, struct Package *package){
	bzero(package, sizeof(struct Package));
	bzero(clientInfo, sizeof(struct sockaddr_in));

    int clientInfoLen = sizeof(*clientInfo);
	int result = recvfrom(socket, package, sizeof(struct Package), 0, clientInfo, &clientInfoLen);
	if (result < 0 ) {
		fprintf(stdout,"%s\n", "Не удалось считать пакет!");
		return -1;
	}

	fprintf(stdout,"Получен пакет на сокете %d:\n  ID - %d\n  MAX_ID - %d\n  CODE - %d\n  DATA - %s\n", 
		socket, package->id, package->maxId, package->code, package->data);

	return result;
}

/**
Формирует паакет и отправляет его.
Входные значения:
	int socket - сокет-дескрипотр соединения;
	struct Package package - пакет.
Возвращаемое значение:
	Количество отправленных байт или -1 если возникла ошибка.
*/
int sendPack(const int socket, const struct sockaddr_in clientInfo, const struct Package package) {
	int len = sizeof(struct sockaddr_in);

	int res = sendto(socket, &package, sizeof(struct Package), 0, &clientInfo, sizeof(struct sockaddr_in));
	if(res < 0 ){
		fprintf(stdout, "Не удалось отправить пакет через сокет %d!\t%d\n", socket, ntohs(clientInfo.sin_port));
		return -1;
	}
	return res;
}

#endif
