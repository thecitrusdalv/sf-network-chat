//argsProcessing.h
#pragma once

#include <iostream>
#include <cstring>
#include <arpa/inet.h>

//Функция проверяет правильность сетевого адреса, пишет результат во второй параметр
bool addressCheck(const char* address, in_addr& addr)
{
	return (inet_aton(address, &addr));
}

//Функция проверяет правильность порта, пишет результат во второй параметр
bool portCheck(const char* port, int &p)
{
	if ((p = atoi(port))) {
		return (p > 0 && p < 65535) ? true : false;
	} else {
		return false;
	}
}

/*
 * Ф-ия обходит аргументы командной строки, если находит соответствие определенным ключам,
 * проверяет аргументы с помощью функций выше
*/
bool argsProc(int argc, const char* const* argv, sockaddr_in &server, sockaddr_in &client)
{
	if (argc <= 1)
		return true;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-sa")) { //Если аргумент соответствует "-sa"
			if (i+1 < argc) { //Есть ли еще аргумент?
				in_addr addr;
				if (addressCheck(argv[i+1], addr)) { //Проверка адреса. Если верный, запись в структуру
					server.sin_addr = addr;
				} else {
					std::cerr << "server address incorrect" << std::endl;
					return false;
				}
			} else {
				return false; //Если аргумента нет, возвврат false
			}
		}

		//Тоже самое
		if (!strcmp(argv[i], "-sp")) {
			if (i+1 < argc) {
				int port;
				if (portCheck(argv[i+1], port))
					server.sin_port = htons(port);
				else {
					std::cout << "server port incorrect" << std::endl;
					return false;
				}
			} else {
				return false;
			}
		}

		//Тоже самое
		if (!strcmp(argv[i], "-cp")) {
			if (i+1 < argc) {
				int port;
				if (portCheck(argv[i+1], port))
					client.sin_port = htons(port);
				else {
					std::cout << "client port incorrect" << std::endl;
					return false;
				}
			} else {
				return false;
			}
		}
	}

	return true;
}
