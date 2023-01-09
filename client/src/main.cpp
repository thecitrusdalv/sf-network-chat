//main.cpp
#include "../headers/client.h"
#include "../headers/argsProcessing.h"

int main(int argc, char **argv)
{
	try {
		//Дефолтные адреса сервера и слушающего потомка
		sockaddr_in server {AF_INET, htons(DEFAULT_SERVER_PORT), inet_addr("127.0.0.1")};
		sockaddr_in listen {AF_INET, htons(DEFAULT_LISTEN_PORT), htonl(INADDR_ANY)};

		//Обработка аргументов переданных в командной строке
		if (!argsProc(argc, argv, server, listen))
			throw "invalid arguments";

		Client client(server, listen);
		client.start(); //Запуск
	}

	catch (std::exception &ex)
	{
		std::cerr << ex.what() << std::endl;
	}

	catch (const char* str)
	{
		std::cerr << "exception: " << str << "\nexit" << std::endl;
	}

	return 0;
}
