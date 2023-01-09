//client.cpp
#include "../headers/client.h"

using std::string;

//Шаблонная функция очищает очередь
template <typename TYPE>
void clearQueue(std::queue<TYPE> &que)
{
	while (!que.empty())
		que.pop();
}

//Проверяет ответ сервера. При ответе сервером в начале сообщения "false:",
//возвращает false.
bool requestIsSuccess(const char* buf)
{
	return strncmp(buf, "false:", 6);
}

Client::Client(const sockaddr_in &server, const sockaddr_in &listen) : serverAddress{server}, myListeningAddress{listen}
{
	bzero(client_buffer, CLIENT_BUFFER_SIZE); //Обнуление сетевого буфера
}

Client::~Client() = default;

/*
 * Ф-ия устанавливает соединение с сервером, отправляет содержимое буфера
 * на сервер, далее ждет ответа через этот же сокет и размещает в том же
 * буфере. После ответа обновляет поле lastRequestStatus
 * В соответствии с "протоколом" сервер обязан в ответе, в начале вернуть
 * статус обработки, завершая его символом ':'. Возможные варианты "false:", "true:", "descr:"
 * Логика программы в дальнейшем основывается на возврате этой ф-ии
*/
bool Client::sendAndRecv(char buf[CLIENT_BUFFER_SIZE])
{
	int socket_to_server = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_to_server == -1) {
		std::cerr << "Cannot create socket to server" << std::endl;
		return false;
	}

	int status = connect(socket_to_server, (sockaddr*) &serverAddress, sizeof(serverAddress));
	if (status == -1) {
		std::cerr << INFO_MESSAGE << "Cannot connect to server" << std::endl;
		close(socket_to_server);
		return false;
	}

	status = write(socket_to_server, buf, CLIENT_BUFFER_SIZE);
	if (status == -1) {
		std::cerr << "Cannot send request to server" << std::endl;
		close(socket_to_server);
		return false;
	}

	status = read(socket_to_server, buf, CLIENT_BUFFER_SIZE);
	if (status == -1) {
		std::cerr << "Cannot get answer from server" << std::endl;
		close(socket_to_server);
		return false;
	}

	if (status == 0) {
		std::cerr << INFO_MESSAGE << "Server disconnected" << std::endl;
		close(socket_to_server);
		return false;
	}
	close(socket_to_server);

	lastRequestStatus.clear();
	char* pos = client_buffer;
	while (*pos != ':') {
		lastRequestStatus += *pos++;
		if (pos == client_buffer + CLIENT_BUFFER_SIZE)
			return false;
	}
	lastRequestStatus += ":";

	//Если статус обработки запроса неудачный, вывод сообщения сервера
	if (!requestIsSuccess(client_buffer)) {
				std::cerr << INFO_MESSAGE << "Server returned: " <<
					client_buffer + lastRequestStatus.size()+1 << std::endl;
			return false;
	} else
		return true;
}

/*
 * Функция размещает в сетевом буфере запрос в правильном формате, для его отправки.
 * Принимает на вход сам буфер для работы, наименование запроса(из перечисления) и
 * очередь из строк, которые нужно добавить в запрос, в соответствующем порядке, поочереди
 * извлекая из очереди. Все сущности разделяются пробельным символом.
*/
void Client::makeRequestInBuffer(char* buf, possibleRequests reqName, std::queue<std::string> &que)
{
	int cur_pos = sprintf(buf, "%i", reqName); //Преобразование enum в текстовый вид
	buf[cur_pos++] = ' ';

	//Цикл добавляет строки в буфер до тех пор, пока очередь не пуста, разделяя их пробелом
	while (!que.empty() && cur_pos < CLIENT_BUFFER_SIZE) {
		strcpy(buf+cur_pos, que.front().data());
		cur_pos += que.front().size();
		buf[cur_pos++] = ' ';
		que.pop();
	}
	buf[cur_pos] = '\0'; //Завершение сообщения
}

static std::queue<string> forms; //Очередь строк для размещения в сетевом буфере

//Старт клиента
void Client::start()
{
	string choise; //Переменная для выбора пользователем

	for (;;) {
		std::cout << "login|register|exit: ";
		std::cin >> choise; //Запрос на ввод
		std::cin.ignore(1000, '\n');

		if (choise == "login" || choise == "log") {

			/*
			 * Если запрос на аутентификацию прошел удачно, проверяется статус на соответствие "descr:"
			 * Если это не так, ответ сервера на запрос игнорируется, иначе обновляется дескриптор,
			 * присланный сервером после "descr: "
			*/
			if (authentication()) {
				if (strncmp(client_buffer, "descr:", 6)) {
					std::cerr << INFO_MESSAGE << "Undefined answer from server" << std::endl;
					continue;
				} else {
					descriptor.clear();
					char* pos = client_buffer+strlen("descr: ");
					while (*pos != '\0' && pos != client_buffer + CLIENT_BUFFER_SIZE)
						descriptor += *pos++;
				}

				//Запуск отдельного процесса для приёма сообщений с сервера
				startListenServer();

				//Вход в пространство пользователя
				userSpace();
			} else {
				std::cerr << INFO_MESSAGE << "Authentication not complete" << std::endl;
			}
			continue;
		}

		if (choise == "register" || choise == "reg") {
			//Если запрос не успешен, переход в начало цикла.
			if (registration()) {
				std::cout << INFO_MESSAGE << "User added" << std::endl;
			} else {
				std::cerr << INFO_MESSAGE << "Registration not complete" << std::endl;
			}
			continue;
		}

		if (choise == "exit") {
			break;
		}

		if (std::cin.fail()) {
			throw "cin fail. Client::connect()";
		}
	}
}

//Запрос на регистрацию
bool Client::registration()
{
	clearQueue<string>(forms); //очищение очереди

	std::cout << "\tType \"back\" to cancel" << std::endl;
	for (;;) {
		string input;
		//Ввод логина
		std::cout << "Login: ";
		std::cin >> input;
		std::cin.ignore(1000, '\n');

		if (input == "back")
			return false;

		if (std::cin.fail())
			throw "cin fail. Client::registrationt()";
		forms.push(std::move(input)); //Добавление в очередь на дальнейшую отправку

		//Ввод пароля
		std::cout << "Password: ";
		std::cin >> input;
		std::cin.ignore(1000, '\n');

		if (input == "back")
			return false;

		if (std::cin.fail())
			throw "cin fail. Client::registrationt()";
		forms.push(std::move(input)); //Добавление в очередь на дальнейшую отправку

		//Ввод nickname
		std::cout << "Nickname: ";
		std::cin >> input;
		std::cin.ignore(1000, '\n');

		if (input == "back")
			return false;

		if (std::cin.fail())
			throw "cin fail. Client::registrationt()";
		forms.push(std::move(input)); //Добавление в очередь на дальнейшую отправку

		//Очередь сформирована следующим образом: логин, пароль, никнейм
		//Размещение запроса в буфере
		makeRequestInBuffer(client_buffer, REGISTRATION, forms);

		//Отправка на сервер
		return sendAndRecv(client_buffer);
	}
}

bool Client::authentication()
{
	clearQueue<string>(forms);

	std::cout << "\tType \"back\" to cancel" << std::endl;
	for (;;) {
		string input;
		std::cout << "Login: ";
		std::cin >> input; std::cin.ignore(1000, '\n');
		if (input == "back")
			return false;
		forms.push(std::move(input));

		std::cout << "Password: ";
		std::cin >> input; std::cin.ignore(1000, '\n');
		if (input == "back")
			return false;
		forms.push(std::move(input));

		if (std::cin.fail())
			throw ("cin fail. Client::logging() input");

		//Очередь сформирована следующим образом: логин, пароль

		//Размещение запроса в буфере
		makeRequestInBuffer(client_buffer, AUTHENTICATION, forms);

		//Отправка запроса на сервер
		return sendAndRecv(client_buffer);
	}
}


//Обертка запроса актуальных сведений об аккаунте
bool Client::mySelfRequest()
{
	clearQueue<string>(forms);
	forms.push(descriptor);

	makeRequestInBuffer(client_buffer, MYSELF, forms);

	if (sendAndRecv(client_buffer)) {
		actualLogin.clear();
		char* pos = client_buffer + lastRequestStatus.size()+1;

		while (*pos != ' ') {
			actualLogin += *pos++;
			if (pos == client_buffer + CLIENT_BUFFER_SIZE)
				return false;
		}

		actualNickname.clear();
		pos++;
		while (*pos != '\0') {
			actualNickname += *pos++;
			if (pos == client_buffer + CLIENT_BUFFER_SIZE)
				return false;
		}

		return true;
	} else
		return false;
}

//Оберка запроса на отправку общего сообщения
bool Client::globalMessageRequest(std::string& input)
{
	//Формирование очереди в порядке: дескриптор, сообщение
	clearQueue<string>(forms);
	forms.push(descriptor);
	forms.push(input);

	makeRequestInBuffer(client_buffer, GLOBAL_MESSAGE, forms);
	if (!sendAndRecv(client_buffer)) {
		std::cerr << INFO_MESSAGE << "Cannot send common message" << std::endl;
		return false;
	}

	return true;
}

//Обертка запроса на вывод личных сообщений
bool Client::showPersonalMessages()
{
	clearQueue<string>(forms);
	forms.push(descriptor);

	makeRequestInBuffer(client_buffer, PERSONAL_MESSAGES, forms);
	if (sendAndRecv(client_buffer)) {
		std::cout << client_buffer + lastRequestStatus.size()+1 << std::endl;
		return true;
	} else {
		std::cerr << "Cannot load private messages from server" << std::endl;
		return false;
	}
}

//Обертка запроса на вывод общих сообщений
bool Client::showGlobalMessages()
{
	//Очистка очереди, формирование очереди в порядке: дескриптор, кол-во сообщений для показа
	clearQueue<string>(forms);
	forms.push(descriptor);
	forms.emplace(std::to_string(DEFAULT_SHOW_MESSAGES_COUNT));

	//Формирование запроса в буфере, отправка
	makeRequestInBuffer(client_buffer, ALL_MESSAGES, forms);

	if (sendAndRecv(client_buffer)) {
		std::cout << client_buffer + lastRequestStatus.size()+1;
		return true;
	} else
		return false;
}

//Обертка запроса на вывод пользователей сервера
bool Client::showUsers()
{
	clearQueue<string>(forms);
	forms.push(descriptor);
	makeRequestInBuffer(client_buffer, LIST_USERS, forms);

	if (sendAndRecv(client_buffer)) {
		std::cout << client_buffer + lastRequestStatus.size()+1;
		return true;
	}
	else
		return false;
}

//Обертка запроса(уведомления) об отключении от сервера
void Client::disconnect()
{
	if (childPid > 0) { //Если есть потомок, слушающий сервер, снять его
		int status = kill(childPid, SIGKILL);
		if (status == -1) {
			std::cerr << INFO_MESSAGE << "cannot kill descendant" << std::endl;
			std::cerr << INFO_MESSAGE << strerror(errno) << std::endl;
		}
		childPid = -1;
	}

	//Очистка очереди, отправка запроса(уведомления) об отключении
	clearQueue<string>(forms);
	forms.push(descriptor);
	makeRequestInBuffer(client_buffer, DISCONNECT, forms);

	if (sendAndRecv(client_buffer)) {
		std::cout << INFO_MESSAGE << "disconnected correctly" << std::endl;
	} else
		std::cerr << INFO_MESSAGE << "disconnected not correctly" << std::endl;

	descriptor.clear(); //Очищение выданного дескриптора
	wait4(-1, nullptr, WNOHANG, nullptr); //Снятие убитого процесса-зомби из таблицы процессов ОС
}

/*
 * Пространство пользователя
 * Работа функции основана на главном бесконечном цикле
 * В каждой итерации пользователь приглашается к вводу
 * На основе введеной строки, определяется команда это
 * или нет.
 * Строка определяется как команда при наличии в начале
 * введенной строки символа :
 * Далее строка разделяется на саму команду и аргументы
 * к ней.
 * В case-ах оператора выбора switch описаны соответсующие
 * обработки команд.
 *
 * Если введенная строка - не команда, просто отправляется
 * запрос на отправку общего сообщения
*/
void Client::userSpace()
{
	//Запрос собственных сведений
	if (!mySelfRequest()) {
		std::cerr << "Cannot get info from server about account" << std::endl;
		return;
	}
	//Показ общих сообщений на сервере
	system("clear"); //очищение экрана перед выводом
	if (!showGlobalMessages()) {
		std::cerr << "Cannot load global messages from server" << std::endl;
		return;
	}

	//Возможные команды в пространстве пользователя
	std::vector<string> commands = {
		"list", "user", "priv", "help"
	};
	enum enumCommands {
		LIST, USER, PRIV, HELP
	};

	//Главный цикл userSpace()
	for (;;) {
		//Если по каким то причинам, потомок, слушающий сервер, убит, прерывание работы
		int status = wait4(-1, nullptr, WNOHANG, nullptr);
		if (status > 0) {
			childPid = -1;
			std::cerr << INFO_MESSAGE << "ERROR: descendant process destroyed" << std::endl;
			break;
		}

		string input; //Строка для ввода

		//Приглашение к вводу
		std::cout << "->" << actualNickname << ": ";
		std::getline(std::cin, input);
		if (std::cin.fail()) {
			disconnect();
			throw ("cin fail. Client::userSpace()");
		}

		if (input[0] == ':') {
			string temp; //Заводится временная строка.
			std::vector<string> CommandArgsVec; //Заводится вектор с командой и ее аргументами.

			//Данный цикл разделяет ввод на отдельные слова, добавляя их в вектор
			//CommandArgsVec
			for (size_t i = 1; i < input.size(); i++) {
				if (input[i] == ' ') {
					if (!temp.empty()) {
						CommandArgsVec.push_back(temp);
						temp.clear();
					}
				} else {
					temp += input[i];
				}
			}
			if (!temp.empty()) {
				CommandArgsVec.push_back(temp);
				temp.clear();
			}

			//Команда :out или :q осуществляет выход из учетной записи, путем прерывания главного цикла
			//пространства пользователя с отправкой уведомления об отключении
			if ((CommandArgsVec[0] == "out") || (CommandArgsVec[0] == "q")) {
				disconnect();
				break;
			}

			//Выход из программы с отправкой уведомления об отключении
			if (CommandArgsVec[0] == "exit") {
				disconnect();
				exit(0);
			}

			int command_number = -1;
			for (size_t i = 0; i < commands.size(); i++) {
				if (CommandArgsVec[0] == commands[i]) {
					command_number = i;
				}
			}
			//Определение команды
			switch (command_number) {
				case LIST: //Вывод пользователей
					showUsers();
					break;
				//Определяет дальнешие действия с определенным юзером на сервере
				case USER: 
					if (CommandArgsVec.size() < 2) { //Если аргументов команды недостаточно
						std::cout << "\tCommand <user> must have at least one argument" << std::endl;
						break;
					}
					{
						string message;
						//Если аргумент команды один, он используется как идентификатор пользователя
						if (CommandArgsVec.size() == 2) {
							std::cout << "\tMessage to " << CommandArgsVec[1] << ": ";
							std::getline(std::cin, message);
						}
						//Если аргументов два и более, начиная со второго, они идентифицируются как
						//сообщение пользователю
						if (CommandArgsVec.size() > 2) {
							for (size_t i = 2; i < CommandArgsVec.size(); i++) {
								message += CommandArgsVec[i];
								if (i != CommandArgsVec.size()-1)
									message += ' ';
							}
						}
						//Формирование очереди в необходимом порядке (дескриптор, никнейм, сообщение)
						clearQueue<string>(forms);
						forms.push(descriptor);
						forms.push(CommandArgsVec[1]);
						forms.push(message);

						//Формирование запроса в буфере и его отправка
						makeRequestInBuffer(client_buffer, USER_MESSAGE, forms);
						if (sendAndRecv(client_buffer))
							std::cout << INFO_MESSAGE << "Private message sended" << std::endl;
						else
							std::cerr << INFO_MESSAGE << "Cannot send private message" << std::endl;
					}
					break;

				case PRIV: //Показ приватных сообщений
					showPersonalMessages();
					break;

				case HELP: //Вывод справки
					std::cout << "\tPossible commands:\n" <<
					"\t:list\t - listing of all users in current room;\n" <<
					"\t:user\t - send private message to other users " <<
						"(:user <name> or :user <name> <message>)\n" <<
					"\t:priv\t - show private messages from other users\n\n" <<
					"\t:out/q\t - logout from current user\n" <<
					"\t:exit\t - exit from program" << std::endl;
					break;

				default:
					std::cout << "\tCommand not found" << std::endl;
					break;
			}

		} else {
			//Если небыло команд, добавление общего сообщения на сервер
			if (!globalMessageRequest(input)) {
				std::cerr << "Cannot send message to server" << std::endl;
				return;
			}

			system("clear");
			if (!showGlobalMessages())
				return;
		}

	}
}

//Обертка для запуска потомка, слушающего сервер
void Client::startListenServer()
{
	int pid = fork();
	if (pid == 0) {
		char feedbackBuffer[FEEDBACK_BUFFER_SIZE]; //Сетевой буфер потомка
		bzero(feedbackBuffer, FEEDBACK_BUFFER_SIZE);

		for (;;) {
			//Если родительский процесс снят,
			//выход (процесс init с pid равным 1 взял обязанности родителя на себя)
			if (getppid() == 1)
				exit(1);

			int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
			if (listen_socket == -1) {
				std::cerr << INFO_LISTENING << "cannot get socket to listen server" << std::endl;
				exit (1);
			}

			int opt = 1;
			setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

			int status = bind(listen_socket, (sockaddr*) &myListeningAddress, sizeof(myListeningAddress));
			if (status == -1) {
				std::cerr << INFO_LISTENING << "cannot bind socket to listen server" << std::endl;
				close(listen_socket);
				exit(1);
			}

			status = listen(listen_socket, 5);
			if (status == -1) {
				std::cerr << INFO_LISTENING << "cannot start listen socket" << std::endl;
				close(listen_socket);
				exit(1);
			}

			//Ожидание соединения
			int server_socket = accept(listen_socket, nullptr, nullptr);
			if (server_socket == -1) {
				std::cerr << INFO_LISTENING << "cannot get socket with server" << std::endl;
				close(listen_socket);
				exit(1);
			}

			int count = recvfrom(server_socket, feedbackBuffer, FEEDBACK_BUFFER_SIZE, 0, nullptr, nullptr);
			if (count) {
				feedbackBuffer[count] = '\0';
				std::cout << "\n" << feedbackBuffer << std::endl;
			}
				close(server_socket);
				close(listen_socket);
		}
		exit(0);
	}
	childPid = pid; //Обновление поля childPid (pid потомка)
}
