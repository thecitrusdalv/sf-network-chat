//client.h
#pragma once

#include <iostream>
#include <vector>
#include <queue>
#include <exception>

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../server/headers/server.h"

#define DEFAULT_SHOW_MESSAGES_COUNT 30 //кол-во глобальных сообщений, которое клиент просит показать сервер
#define CLIENT_BUFFER_SIZE 2048 //Размер сетевого буфера обмена
#define FEEDBACK_BUFFER_SIZE 2048 //Размер сетевого буфера, потока, слушающего сервер

#define INFO_MESSAGE "\t-> " //Заглавие информационных сообщений
#define INFO_LISTENING "\tERROR while listening server: " //Заглавие информационных сообщений слушающего потока
#define DEFAULT_SERVER_PORT 60000
#define DEFAULT_LISTEN_PORT 60005 //Дефолтный порт, на котором клиент слушает сервер

class Client
{
public:
	Client(const sockaddr_in&, const sockaddr_in&);
	~Client();

	void start(); //Подключение к серверу

private:
	sockaddr_in serverAddress; //Адрес сервера
	sockaddr_in myListeningAddress; //Адрес слушающего потока

	char client_buffer[CLIENT_BUFFER_SIZE]; //Сетевой буфер

	std::string descriptor; //Дескриптор выданный сервером
	std::string actualNickname, actualLogin; //Актуальные собственные сведения, получаемые от сервера, пишутся сюда

	bool registration(); //Обертка запроса для регистрации
	bool authentication(); //Обертка запроса аутентификации
	void userSpace(); //Пространство пользователя
	bool showUsers(); //Запрос на вывод всех пользователей
	bool showPersonalMessages(); //Обертка запроса личных сообщений
	bool showGlobalMessages(); //Обертка запроса общих сообщений
	bool mySelfRequest(); //Запрос на получение сведений о своем аккакунте
	bool globalMessageRequest(std::string&); //Запрос на отправку общего сообщения в чат
	void startListenServer(); //Обертка для запуска потока, слушающего сервер
	void disconnect(); //Отправка уведомления на сервер об отключении


	std::string lastRequestStatus; //Значение статуса последнего запроса, возвращенного сервером

	//help functions
	bool sendAndRecv(char*); //ф-ия обеспечивает отправку и приём сообщений в сетевой буфер

	/*
	 * Функция makeRequestInBuffer размещает в правильном формате запрос в сетевом буфере
	 * перед отправкой
	*/
	void makeRequestInBuffer(char*, possibleRequests, std::queue<std::string>&);

	int childPid = -1; //Поле хранит PID слушающего потока. -1 Означает что потока нет.
};
