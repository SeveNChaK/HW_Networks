#ifndef DEXCHANGE_H
#define DEXCHANGE_H

#define QUANTITY_TRY 10

int safeReadMsg(const int socket, const sockaddr_in *clientInfo, const Message *msg) {
	bzero(msg, sizeof(struct Message));

	struct Package package;

	int nextId = 1;
	int expectedMaxId = 0;
	int expectedCode = 0;
	int wrongPack = QUANTITY_TRY;
	for (;;) { //Ждем первый пакет
		if (readPack(socket, clientInfo, &package) < 0) {
            logError("Ошибка при надежном чтении сообщения!\n");
            return -1;
        }

        if (package.id == nextId) {
        	expectedMaxId = package.maxId;
        	expectedCode = package.code;
        	break;
        } else {
        	if (--wrongPack == 0) {
        		logError("Получено много неверных пакетов при ожидании первого. Предположительно что-то не так с сетью. Сообщение не получено.\n");
        		return -1;
        	}
        }
	}

	wrongPack = QUANTITY_TRY;
	for(;;) {

        if (package.maxId == expectedMaxId 
        	&& package.code == expectedCode 
        	&& package.id == nextId
        ) {
        	msg->type = package.code;
        	msg->length += package.lengthData;
        	strcat(msg->data, package.data);

        	package.acc = ACK;
        	if (sendPack(socket, *clientInfo, package) < 0) {
        		logError("Ошибка отправки подтверждения!\n");
            	return -1;
        	}

        	nextId++;
        } else {
        	logDebug("Получен не верный пакет!");

        	sendPack(socket, *clientInfo, package);

        	if (--wrongPack == 0) {
        		logError("Получено много неверных пакетов. Предположительно что-то не так с сетью. Сообщение не получено.\n");
        		return -1;
        	}
        }

        if (package.id == expectedMaxId) {
        	break;
        }

        //СДЕЛАТЬ СЕЛЕКТОМ ТАЙМ-АУТ ПО ВРЕМНИ
        if (readPack(socket, clientInfo, &package) < 0) {
            logError("Ошибка при надежном чтении сообщения!\n");
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
				const struct sockaddr_in clientInfo,
				const int code,
				const char *msg,
				const int lengthMsg
) {
	int tempQuantityPacks = lengthMsg / SIZE_PACK_DATA;
	int lengthLastPack = lengthMsg % SIZE_PACK_DATA;
	int quantityPacks = tempQuantityPacks;
	if (lengthLastPack != 0 || tempQuantityPacks == 0) {
		quantityPacks += 1;
	}

	int sendedPacks = 0;
	struct Package currentPackage;
	struct Package lastPackage;
	struct sockaddr_in fromClientInfo;
	bzero(&currentPackage, sizeof(currentPackage));
	int quantityTry = 10;
	for (;;) {

		if (currentPackage.id != sendedPacks + 1) { //Проверка на то, что отправляем - старый или новый пакет
			bzero(&currentPackage, sizeof(currentPackage));
			currentPackage.acc = NO_ACK;
			currentPackage.id = sendedPacks + 1;
			currentPackage.maxId = quantityPacks;
			currentPackage.code = code;
			if (currentPackage.id != quantityPacks) {
				currentPackage.lengthData = SIZE_PACK_DATA;
				memcpy(currentPackage.data, msg, SIZE_PACK_DATA);
				msg += SIZE_PACK_DATA;
			} else {
				currentPackage.lengthData = lengthLastPack;
				memcpy(currentPackage.data, msg, lengthLastPack);
				msg += lengthLastPack;
			}
		}

		if (sendPack(socket, clientInfo, currentPackage) < 0) {
			logError("Ошибка отправки пакета при надежной отправки сообщения!\n");
			return -1;
		}

		//СДЕЛАТЬ СЕЛЕКТОМ ТАЙМ-АУТ ПО ВРЕМНИ
		if (readPack(socket, fromClientInfo, &lastPackage) < 0) {
			logError("Ошибка при чтении подтверждения!\n");
			return -1;
		}

		if (cmpPack(currentPackage, lastPackage) == 0 && lastPackage.ack == ACK) {
			logDebug("Получено подтверждение.");
			sendedPacks++;
			if (sendedPacks > quantityPacks) {
				logDebug("Сообщение полностью отправлено, и полученно клиентом.");
				break;
			}
		} else {
			logDebug("Подтверждение не получено, повторная отправка пакета.");

			if (--quantityTry == 0) {
				logDebug("Слишком много попыток без подтверждения. Возможно проблемы с сетью. Сообщение не отправлено.");
				return -1;				
			}
		}
	}

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
int readPack(const int socket, const struct sockaddr_in *clientInfo, const struct Package *package){
	bzero(package, sizeof(struct Package));
	bzero(clientInfo, sizeof(struct sockaddr_in));

    int clientInfoLen = sizeof(*clientInfo);
	int result = recvfrom(socket, package, sizeof(struct Package), 0, clientInfo, &clientAddrLen);
	if (result < 0 ) {
		logError("%s\n", "Не удалось считать пакет!");
		return -1;
	}

	logDebug("Получен пакет на сокете %d:\n  ID - %d\n  MAX_ID - %d\n  CODE - %d\n  DATA - %s\n", 
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
	int len = sizeof(clientInfo);
	int res = sendto(socket, &package, sizeof(struct Package), 0, &clientInfo, &len);
	if(res < 0 ){
		logError("%s\n", "Не удалось отправить пакет через сокет %d!", socket);
		return -1;
	}
	return res;
}

#endif
