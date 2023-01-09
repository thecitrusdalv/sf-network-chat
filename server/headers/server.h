//server.h
#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <deque>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <string_view>

#define NAME_MAX_SIZE 20 //Максимальное количество символов идентификаторов пользователя
#define MESSAGES_LOAD_COUNT 30 //Кол-во сообщений, подтягиваемых из файла
#define DESCRIPTOR_SIZE 20 //Размер выдаваемого клиентам дескриптора (символов)
#define FILLING_RATIO 1.5 //Коэффициент наполнения очереди глобальных сообщений
#define USERS_FILE "./relations/users" //Файл для хранения пользователей
#define GLOBAL_MESSAGES_FILE "./relations/global_messages" //Файл хранения глобальных сообщений

//Директория для хранения файлов пользователей с личными сообщениями
#define PERSONAL_MESSAGES_PATH "./relations/personal/" 
#define NETWORK_BUFFER_SIZE 2048 //Размер сетевого буфера обмена
#define REQUEST_MAX_SIZE 2 //Максимальный размер идентификатора запроса (в символах)
#define CLIENTS_QUEUE 16 //Максимальное кол-во клиентов, ожидающих подключения
#define PORT 60000 //Дефолтный порт сервера
#define CLIENT_FEEDBACK_PORT 60005 //Дефолтный порт feedback-канала клиента
#define SERVER_LOG_MESSAGE "-> Server: " //Заголовок информационного сообщения сервера

#include "fileSubsystem.h"
#include "networkSubsystem.h"
/*
 * Определение максимального кол-ва сообщений, которое сервер будет хранить в памяти.
 * Зависит от кол-ва сообщений, которое сервер загружает в память из файла и коэффициента.
 * При достижении большего кол-ва, чем максимальное, сервер автоматически высвобождает
 * старые сообщения.
*/
constexpr size_t MESSAGES_MAXIMUM_STORED = MESSAGES_LOAD_COUNT * FILLING_RATIO;

extern char buffer[NETWORK_BUFFER_SIZE]; //Сетевой буфер обмена
extern char* buffer_end; //Указатель на конец сетевого буфера для удобства

using std::string;
namespace fs = std::filesystem;

//Перечисление принимаемых сервером запросов
enum possibleRequests {
	AUTHENTICATION = 1,
	REGISTRATION,
	GLOBAL_MESSAGE,
	USER_MESSAGE,
	PERSONAL_MESSAGES,
	ALL_MESSAGES,
	LIST_USERS,
	MYSELF,
	DISCONNECT,
};

class Server
{
public:
	//Структура сообщений
	struct Message
	{
		Message();
		Message(const string&, const std::string&);
		Message(const Message&);
		Message(Message&&);
		Message& operator= (const Message&);
		Message& operator= (Message&&);
		~Message();

		string own, msg;
	};

	//Структура пользователей
	struct User
	{
		User(const string&, const std::string&, const std::string&);
		User(const User&);
		User(User&&);
		User& operator= (const User&);
		User& operator= (User&&);
		~User();

		string login, nickname, pass;
	};

	Server();
	~Server();
	Server(const Server&);
	Server(const Server&&);
	Server& operator= (const Server&);
	Server& operator= (const Server&&);

	//Обработчики запросов
	bool request(sockaddr_in&);
private:
	/*
	* Файловая подсистема.
	* Осуществляет создание, открытие необходимых для работы файлов.
	* Выставляет необходимые права на файлы.
	* Предоставляет серверу готовые файловые потоки.
	*/
	FileSubsystem fileSubsystem; //Экземпляр класса файловой подсистемы

	/*
	 * Сетевая подсистема.
	 * Организует подготовку к приёму сообщений по сети,
	 * принимает запросы в буфер и вызывает функцию у сервера для их обработки,
	 * отправляет результат обработки обратно
	*/
	NetworkSubsystem networkSubsystem; //Экземпляр класса сетевой подсистемы

	std::map<string, User*> logins; //Карта логинов
	std::map<string, User*> nicknames; //Карта псевдонимов
	std::map<string, string> issuedDescriptors; //Карта выданных дескрипторов
	std::map<string, sockaddr_in> loggedClients; //Карта подключенных пользователей

	std::deque<Message> messages; //Очередь общих сообщений на сервере

	void addUser(const string&, const std::string&, const std::string&); //Добавление нового пользователя
	void loadUserToMemory(const string&, const std::string&, const std::string&); //Подгрузка в память пользователей
	bool loadUsersFromFile(std::fstream&); //Подгрузка пользователей из файла
	void loadGlobalMessagesFromFile(std::fstream&, const size_t); //Подгрузка общих сообщений из файла в память
	bool delUser(const string&); //Удаление пользователя из памяти
	bool name_check(const string&, std::string&) const; //Валидация идентификаторов пользователей
	bool loginExist(const string&) const; //Проверка существования логина
	bool nicknameExist(const string&) const; //Проверка существования псевдонима
	bool descriptorExist(const string&) const; //Проверка существования дескриптора
	bool sendToUser(const string&, const string&); //Отправка сообщения клиенту в feedback канал
};
