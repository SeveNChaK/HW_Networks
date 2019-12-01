## 1.2.13 Новости

### Компиляция
    gcc -pthread <source_file> -o <target_file_name>
    
#### Пример
    gcc -pthread server.c -o server
    
### Запуск
    ./server <PORT>
    ./client <IP_SERVER> <PORT_SERVER>
    
#### Пример
    ./server 8080
    ./client 127.0.0.1 8080
    
### Команды сервера
    /help
    /lс - список клиентов
	/addt - добавление темы
	/kick [NUMBER_CLIENT] - отключение клиента\
	/shutdown - выключение сервера

### Командыы клиента
    /help
    /lst - список доступных тем
	/lsn - список всех новостей
	/lsn [NUMBER_THEME] - список всех новостей по теме
	/addn [NUMBER_THEME] - добавление новости по теме
	    /s - сохранение новости
	    /с - отмена создания новости
	/news [NUMBER_NEWS] - просмотр новости
	/find [SEARCH_STR] - поиск нвостей по названию. Пример: /find Вот так эта команда выглядит в действии
	/disconnect - выключение клиента
