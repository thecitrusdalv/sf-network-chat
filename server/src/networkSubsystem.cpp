//networkSubsystem.cpp
#include "../headers/networkSubsystem.h"
#include "../headers/server.h"

char buffer[NETWORK_BUFFER_SIZE]; //Сетевой буфер обмена
char* buffer_end = nullptr; //Указатель на конец буфера, для удобства

NetworkSubsystem::NetworkSubsystem()
{
	me.sin_family = AF_INET;
	me.sin_port = htons(PORT);
	me.sin_addr.s_addr = htonl(INADDR_ANY);
}

/*
 * Инициализация сетевой подсистемы.
 * Создание слушающего сокета
*/
bool NetworkSubsystem::init()
{
	listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_socket == -1) {
		std::cerr << NETWORK_LOG_MESSAGE << "cannot get socket" << std::endl;
		return false;
	}
	int opt = 1;
	setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	max_fd = listen_socket;

	status = bind(listen_socket, (sockaddr*) &me, sizeof(me));
	if (status == -1) {
		std::cerr << NETWORK_LOG_MESSAGE <<
			"cannot bind listen socket with address" << std::endl;
		return false;
	}

	status = listen(listen_socket, CLIENTS_QUEUE);
	if (status == -1) {
		std::cerr << NETWORK_LOG_MESSAGE <<
			"cannot start listen socket" << std::endl;
		close(listen_socket);
		return false;
	}

	return true;
}

/*
 * Старт сети.
 * Логика работы следующая:
 * Ожидание запросов основано на системном вызове select.
 *
 * Каждый запрос, присылаемый клиентом, обрабатывается отдельным соединением,
 * по окончанию работы обработки запроса, соединение закрывается. Таким образом,
 * сетевая подсистема не допускает большого количества открытых сокетов в системе, а также
 * клиент не зависит от сервера и может сосуществовать отдельно, периодически при необходимости
 * присылая запросы и получая ответы от сервера.
 *
 * В свою очередь, клиент обязан иметь feedback канал (открытое слушающее соединение), для того,
 * чтобы сервер мог присылать информационные сообщения клиенту. Сервер отправляет сообщения только
 * подключенным (авторизованным) пользователям. Подсистема сети не работает с feedback соединением,
 * этим занимается сервер, однако, она предоставляет серверу адрес клиента для установления этого самого
 * feedback соединения, после того как от клиента приходит запрос в подсистему сети.
 *
 * В начале работы заводится односвязный список observable_readfds для сокетов, которые требуется отслеживать
 * Также заводится стек для сокетов, которые требуется исключить из отслеживания
 *
 * На каждой итерации вызов select отслеживает сокеты из списка отслеживаемых сокетов observable_readfds
 * Сразу после начала работы с отслеживаемым сокетом, он включается в стек exclude_r для его исключения на следующей
 * итерации (за исключением слушающего сокета listen_socket).
 *
 * При подключении клиента, сокет, созданный для него, включается в список отслеживамых. При поступлении данных
 * на вновь созданный сокет, сокет добавляется в стек исключаемых сокетов, подсистема сети размещает данные в
 * буфере обмена, вызывает обработчик запроса у сервера, сервер обрабатывает запрос, основываясь на данных в буфере и
 * размещает в этом же буфере ответ. Далее подсистема сети через тот же сокет отправляет данные в буфере клиенту, содержащие результат
 * обработки запроса сервером, и закрывает соединение.
*/
void NetworkSubsystem::start(Server& kernel)
{
	std::forward_list<int> observable_readfds;
	std::stack<int> exclude_r;

	fd_set readfds;
	FD_ZERO(&readfds);

	std::cout << NETWORK_LOG_MESSAGE <<
		"starting listening on port: " << PORT << '\n' << std::endl;

	for (;;) {
		//Пока стек сокетов не пуст, исключать сокеты в нем из отслеживаемых
		while (!exclude_r.empty()) {
			observable_readfds.remove(exclude_r.top());
			exclude_r.pop();
		}

		FD_SET(listen_socket, &readfds);

		//Установка отслеживаемых сокетов
		for (const int x : observable_readfds)
			FD_SET(x, &readfds);

		status = select(max_fd+1, &readfds, nullptr, nullptr, nullptr);
		if (status < 0) {
			if (errno == EINTR) {
				std::cout << NETWORK_LOG_MESSAGE << "interrupt\n";
				return;
			} else {
				std::cerr << NETWORK_LOG_MESSAGE << "undefined error\n";
				return;
			}
		}

		//Если установлено соединение, создание сокета, извлечение адреса клиента через getpeername,
		//добавление вновь созданного сокета в список отслеживаемых
		if (FD_ISSET(listen_socket, &readfds)) {
			int client_socket = accept(listen_socket, nullptr, nullptr);
			if (client_socket == -1) {
				std::cerr << NETWORK_LOG_MESSAGE <<
					"cannot get socket for new client\n";
			} else {
				sockaddr_in temp;
				socklen_t tempSize = sizeof(temp);
				getpeername(client_socket, (sockaddr*)&temp, &tempSize);

				std::cout << NETWORK_LOG_MESSAGE <<
					"client connected, socket: " << client_socket << ", address: " << inet_ntoa(temp.sin_addr) << '\n';
				if (max_fd < client_socket)
					max_fd = client_socket;
				observable_readfds.push_front(client_socket);
			}
		}

		//При появлении данных в отслеживаемом сокете
		for (const int current_socket : observable_readfds) {
			if (FD_ISSET(current_socket, &readfds)) {

				//Исключение сокета из отслеживаемых
				exclude_r.push(current_socket);
				FD_CLR(current_socket, &readfds);

				sockaddr_in current_client;
				socklen_t client_size = sizeof(current_client);

				int count = recvfrom(current_socket, ::buffer, NETWORK_BUFFER_SIZE, 0, (sockaddr*)&current_client, &client_size);

				if (count > 0) {
					buffer[count] = '\0';
					std::cout << NETWORK_LOG_MESSAGE << "client message: [" << ::buffer << "]"<< std::endl;

					//Извлечение адреса клиента через getpeername
					sockaddr_in client_addr;
					socklen_t client_addr_size = sizeof(client_addr);
					getpeername(current_socket, (sockaddr*) &client_addr, &client_addr_size);

					//Вызов обработчика запросов сервера, передвая ему адрес клиента
					bool request_status = kernel.request(client_addr);

					//По окончанию обработки сервером, отправка результата обработки обратно
					write(current_socket, ::buffer, NETWORK_BUFFER_SIZE);

					std::cout << NETWORK_LOG_MESSAGE << "server processing " <<
						(request_status ? "*** SUCCESS ***" : "*** FAILED ***") << '\n';

					//Закрытие соединения
					std::cout << NETWORK_LOG_MESSAGE << "connection closed\n";
					close(current_socket);
				}
				else
					std::cout << NETWORK_LOG_MESSAGE <<
						"cannot read client socket. Socket: " << current_socket << '\n';
				std::cout << "-----\n" << NETWORK_LOG_MESSAGE << "waiting..." << std::endl;
			}
		}
	}
}
