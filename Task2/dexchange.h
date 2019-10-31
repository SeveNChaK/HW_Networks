#ifndef DEXCHANGE_H
#define DEXCHANGE_H

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
int readPack(int socket, struct Package *package){
	bzero(package->data, sizeof(package->data));
	int result = readN(socket, package, sizeof(struct Package));
	if(result < 0 ){
		fprintf(stderr, "%s\n", "Не удалось считать пакет!");
		return -1;
	}
	fprintf(stdout, "Получен пакет:\n\tкод - %d\n\tданные - %s\n", package->code, package->data);
	return result;
}

/**
Формирует паакет и отправляет его.
Входные значения:
	int socket - сокет-дескрипотр соединения;
	int code - код отправляемого пакета;
	char *data - ссылка на строку, которая содержит отправляемые данные.
Возвращаемое значение:
	Количество отправленных байт или -1 если возникла ошибка.
*/
int sendPack(int socket, int code, int sizeData, char *data){
	struct Package package;
	bzero(package.data, sizeof(package.data));
	package.code = code;
	package.sizeData = sizeData;
	memcpy(package.data, data, sizeData);
	// strcpy(package.data, data);
	int res = send(socket, &package, sizeof(struct Package), 0);
	//TODO обработать ошибку
	return res;
}

/**
Функиця читает N байт из сокета. Поток будет заблокирован на этой функции, 
пока не будет считано заданное количество байт или пока не произойдет
закрытие сокета.
Входные значения:
	int socket - сокет-дескриптор, из которого будут считаны данные;
	char *buf - строка, куда будут считаны данные;
	int length - количество байт, которое необходимо считать.
Возвращаемое значение:
	Количество считанных байт или -1 если не удалось считать данные.
*/
int readN(int socket, char* buf, int length){
	int result = 0;
	int readedBytes = 0;
	int sizeMsg = length;
	while(sizeMsg > 0){
		readedBytes = recv(socket, buf + result, sizeMsg, 0);
		if (readedBytes <= 0){
			return -1;
		}
		result += readedBytes;
		sizeMsg -= readedBytes;
	}
	return result;
}

#endif
