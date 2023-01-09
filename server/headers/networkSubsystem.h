//networkSubsystem.h
#pragma once

#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <forward_list>
#include <stack>

#define NETWORK_LOG_MESSAGE "-> Network subsystem: " //Заголовок информационного сообщения

class Server;
class NetworkSubsystem
{
public:
	NetworkSubsystem();
	bool init(); //Инициализация подсистемы
	void start(Server&); //Старт сети
private:
	sockaddr_in me; //Собственный адрес
	int listen_socket, status, max_fd;
};
