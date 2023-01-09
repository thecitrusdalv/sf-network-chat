//server.cpp
#include "../headers/server.h"

using std::string;

Server::Server()
{
	//Инициализация фаловой подсистемы. Если неудачно, выброс исключения
	if (fileSubsystem.init()) {

		loadUsersFromFile(fileSubsystem.usersFile); //Загрузка пользователей из файла в память

		//Загрузка глобальных сообщений из файла в память.
		loadGlobalMessagesFromFile(fileSubsystem.globalMessagesFile, MESSAGES_LOAD_COUNT);

	} else
		throw "Can't initialize file subsystem";

	//Инициализация сетевой подсистемы
	if(!networkSubsystem.init())
		throw "Can't initialize network subsystem";

	//Старт сетевой подсистемы
	networkSubsystem.start(*this);
}

Server::~Server()
{
	//Освобождение памяти от пользователей
	for (auto x : logins) {
		delete x.second;
	}
}

Server::Server(const Server&)	= delete;
Server::Server(const Server&&)	= delete;

Server& Server::operator= (const Server&)	= delete;
Server& Server::operator= (const Server&&)	= delete;

//Вспомогательные функции

//Возвращает строку с определенным словом из буфера
std::string getBufferSection(std::vector<char*>& vec, size_t n) {
	std::string_view str;
	if (n > vec.size())
		return "";

	str = vec[n];
	auto trim_pos = str.find(' ');
	if (trim_pos != str.npos)
		str.remove_suffix(str.size() - trim_pos);

	return static_cast<std::string>(str);
}

//Возвращает строку с содержимым буфера, начиная с определенного слова
std::string getBufferLeftOver(std::vector<char*>& vec, size_t n) {
	std::string_view str;
	if (n > vec.size())
		return "";

	str = vec[n];
	auto trim_pos = str.find('\0');
	if (trim_pos != str.npos)
		str.remove_suffix(str.size() - trim_pos);

	return static_cast<std::string>(str);
}

/*
 * Функция обработки запросов.
 * Функция принимает адрес клиента, от которого пришел запрос. В адресе меняется порт
 * на слушающий порт у клиента.
 * В дальнейшем, при успешной авторизации этот адрес кладется в дерево авторизованных
 * пользователей для связи с пользователями, находящимися "онлайн".
 *
 * Сервер читает сетевой буфер. Самым первым в буфере должен лежать
 * идентификатор запроса.
 *
 * Если запрос распознан, происходит его дальнейшая обработка в соответствующем
 * case-е оператора switch.
 *
 * По окончанию обработки запроса, сервер кладет ответ обратно в тот же буфер,
 * отдает управление подсистеме сети.
 *
 * При необходимости осуществляется отправка сообщений пользователям "онлайн"
 * через feedback соединение.
 *
 * В каждом запросе, за исключением аутентификации, должен быть
 * дескриптор, который выдается сервером после аутентификации.
 * Дескриптор должен лежать сразу после идентификатора запроса.
 *
*/

bool Server::request(sockaddr_in &client_addr)
{
	client_addr.sin_port = htons(CLIENT_FEEDBACK_PORT); //Меняем порт адреса клиента на нужный

	buffer_end = buffer+NETWORK_BUFFER_SIZE; //Указатель на конец сетевого буфера для удобства

	std::vector<char*> bufferSections; //Заводится вектор с указателями на секции буфера (отдельные слова)
	bufferSections.reserve(20);

	//Нахождение в буфере отдельных слов и добавление указателей на их начала в вектор bufferSections.
	bool sectionBeginFound = false;
	for (char *current_pos = buffer; current_pos < buffer_end && *current_pos; current_pos++) {
		if (*current_pos != ' ') {
			if (!sectionBeginFound) {
				bufferSections.push_back(current_pos);
				sectionBeginFound = true;
			}
		} else {
			sectionBeginFound = false;
		}
	}

	//Преобразование первой секции сетевого буфера (идентификатора запроса) в целочисленный тип
	int req_number = atoi(bufferSections[0]);

	switch(req_number) {
		case AUTHENTICATION:
			std::cout << SERVER_LOG_MESSAGE << "authentication request" << std::endl;
			//Структура запроса должна быть вида: ИДЕНТИФИКАТОР ЗАПРОСА - ЛОГИН - ПАРОЛЬ

			//Если кол-во секций в буфере не соответствует требуемому - ошибка, отклонение запроса
			if (bufferSections.size() != 3) {
				std::cerr << SERVER_LOG_MESSAGE << "invalid request format" << std::endl;
				strcpy(buffer, "false: invalid request format"); //Запись результата в сетевой буфер
				return false;
			}

			{
				//Извлечение секций буфера в необходмые поля
				string login = getBufferSection(bufferSections, 1);
				string pass = getBufferSection(bufferSections, 2);

				//Если логин не существует
				if (!loginExist(login)) {
					std::cerr << SERVER_LOG_MESSAGE << "login not found" << std::endl;
					strcpy(buffer, "false: login not found");
					return false;
				}

				/*
				 * Находим в карте итератор, соответствующий логину в запросе,
				 * Вытаскиваем пароль и сравниваем с тем, что в запросе.
				 * В случае успеха, генерируется дескриптор и отправляется клиенту.
				 * Клиент использует дескриптор в дальнейших запросах для идентификации
				*/
				
				auto it = logins.find(login);

				if (pass == it->second->pass) {

					string newDescriptor;
					for (int i = 0; i < DESCRIPTOR_SIZE; i++) {
						newDescriptor += char(rand() % 26 + 'a');
					}


					issuedDescriptors[newDescriptor] = login; //Добавление дескриптора в дерево выданных дескрипторов

					printf("%suser [%s] login success\n", SERVER_LOG_MESSAGE, login.data());

					//Запись результата в буфер
					strcpy(buffer, "descr: ");
					strcpy(buffer + strlen("descr: "), newDescriptor.data());

				} else {
					std::cerr << SERVER_LOG_MESSAGE << "incorrect password" << std::endl;
					strcpy(buffer, "false: incorrect password");
					return false;
				}

				//Если клиент был ранее авторизован, удаление старой записи, запись нового адреса клиента
				//if (loggedClients.find(it->second->login) != loggedClients.end())
					//loggedClients.erase(loggedClients.find(it->second->login));
				loggedClients[it->second->login] = client_addr;
			}
			break;
		case REGISTRATION:
			std::cout << SERVER_LOG_MESSAGE << "registration request" << std::endl;
			//Структура запроса должна быть вида: ИДЕНТИФИКАТОР ЗАПРОСА - ЛОГИН - ПАРОЛЬ - НИКНЕЙМ
			{
				//Проверка на количество необходимых секций в запросе
				if (bufferSections.size() != 4) {
					std::cerr << SERVER_LOG_MESSAGE << "invalid request format" << std::endl;
					strcpy(buffer, "false: invalid request format");
					return false;
				}

				//Извлечение необходимых секций
				string login = getBufferSection(bufferSections, 1);
				string pass = getBufferSection(bufferSections, 2);
				string nickname = getBufferSection(bufferSections, 3);

				string result;
				if (!name_check(login, result)) { //Проверка логина на соответствие требованиям
					std::cerr << SERVER_LOG_MESSAGE << "invalid login" << std::endl;
					strcpy(buffer, "false: ");
					strcpy(buffer+7, result.data());
					return false;
				}

				if (!name_check(nickname, result)) { //Проверка никнейма на соответствие требованиям
					std::cerr << SERVER_LOG_MESSAGE << "invalid nickname" << std::endl;
					strcpy(buffer, "false: ");
					strcpy(buffer+7, result.data());
					return false;
				}

				if (loginExist(login)) { //Если логин уже существует - отклонение запроса
					std::cerr << SERVER_LOG_MESSAGE << "login unavailable" << std::endl;
					strcpy(buffer, "false: login unavailable");
					return false;
				}

				if (nicknameExist(nickname)) { //Если никнейм уже существует - отклонение запроса
					std::cerr << SERVER_LOG_MESSAGE << "nickname unavailable" << std::endl;
					strcpy(buffer, "false: nickname unavailable");
					return false;
				}

				//Добавление пользователя на сервер и отправка результата
				addUser(std::move(login), std::move(pass), std::move(nickname));
					std::cout << SERVER_LOG_MESSAGE << "user added" << std::endl;
					strcpy(buffer, "true: user added");
			}
			break;
		case GLOBAL_MESSAGE:
			std::cout << SERVER_LOG_MESSAGE << "global message request" << std::endl;
			//Структура запроса должна быть вида: ИДЕНТИФИКАТОР ЗАПРОСА - ДЕСКРИПТОР - СООБЩЕНИЕ
			{
				//Проверка на количество необходимых секций в запросе
				if (bufferSections.size() < 3) {
					std::cerr << SERVER_LOG_MESSAGE << "invalid request format" << std::endl;
					strcpy(buffer, "false: invalid request format");
					return false;
				}

				//Извлечение необходимых секций
				string descriptor = getBufferSection(bufferSections, 1);
				string message = getBufferLeftOver(bufferSections, 2);

				//Если такой дескриптор не выдан - отклонение запроса
				if (!descriptorExist(descriptor))
					return false;

				//Находим никнейм пользователя по дескриптору в заросе
				auto& nickname = logins.find(issuedDescriptors[descriptor])->second->nickname;
				//Записываем сообщение в очередь сообщений. Обновляем файл общих сообщений.
				messages.emplace_front(nickname, message);
				fileSubsystem.globalMessagesFile << nickname << ' ' << message << std::endl;

				//Отправка результата обработки запроса
				std::cout << SERVER_LOG_MESSAGE << "message recieved" << std::endl;
				strcpy(buffer, "true: message recieved");

				//Отправка сообщения залогиненым пользователям
				std::stack<string> excludeUserFromLogged; //Стек для исключения пользователей из списка авторизованных
				//Просмотр всех авторизованных пользователей
				for (auto& client : loggedClients) {
					//Пропуск самого себя
					if (client.first == issuedDescriptors[descriptor])
						continue;
					//Попытка установки feedback-соединения
					int socket_to_client = socket(AF_INET, SOCK_STREAM, 0);
					if (socket_to_client == -1) {
						printf("%scannot create socket to user [%s] to send message\n", SERVER_LOG_MESSAGE, client.first.data());
						continue;
					}

					int status = connect(socket_to_client, (sockaddr*) &client.second, sizeof(client.second));
					if (status == -1) {
						printf("%scannot connect to user [%s] to send message\n", SERVER_LOG_MESSAGE, client.first.data());
						//Если подключиться к клиенту не удалось, исключение его из "списка онлайн"
						excludeUserFromLogged.push(client.first);
						close(socket_to_client);
						continue;
					} else {
						//Формирование готового сообщения из никнейма и сообщения и его отправка
						string complete_message = "\t";
						complete_message += nickname;
						complete_message += ": ";
						complete_message += message;

						status = write(socket_to_client, complete_message.data(), complete_message.size());
						if (status != (int)complete_message.size()) {
							printf("%scannot send message to user [%s]\n", SERVER_LOG_MESSAGE, client.first.data());
						} else {
							printf("%smessage to [%s] sended\n", SERVER_LOG_MESSAGE, client.first.data());
						}
					}
					close(socket_to_client);
				}
				//Исключение отключенных пользователей из "списка онлайн"
				while (!excludeUserFromLogged.empty()) {
					loggedClients.erase(loggedClients.find(excludeUserFromLogged.top()));
					excludeUserFromLogged.pop();
				}
			}
			break;
		case USER_MESSAGE:
			std::cout << SERVER_LOG_MESSAGE << "user message request" << std::endl;
			//Структура запроса должна быть вида: ИДЕНТИФИКАТОР ЗАПРОСА - ДЕСКРИПТОР - НИКНЕЙМ ПОЛУЧАТЕЛЯ - СООБЩЕНИЕ
			{
				//Проверка на количество необходимых секций в запросе
				if (bufferSections.size() < 4) {
					std::cerr << SERVER_LOG_MESSAGE << "invalid request format" << std::endl;
					strcpy(buffer, "false: invalid request format");
					return false;
				}
				//Извлечение необходимых секций
				string descriptor = getBufferSection(bufferSections, 1);
				string nickname = getBufferSection(bufferSections, 2);
				string message = getBufferLeftOver(bufferSections, 3);

				//Если такой дескриптор не выдан - отклонение запроса
				if (!descriptorExist(descriptor))
					return false;

				//Если пользователя с таким никнеймом нет - отклонение запроса
				if (!nicknameExist(nickname)) {
					std::cerr << SERVER_LOG_MESSAGE << "user not found" << std::endl;
					strcpy(buffer, "false: user not found");
					return false;
				}

				//Находим отправителя и получателя для извлечения информации из них
				const Server::User& sender = *(logins.find(issuedDescriptors[descriptor])->second);
				Server::User& reciever = *(logins.find(nickname)->second);

				//Кладем адресату персональное сообщение в его файл
				std::fstream recieverFile;
				if (!fileSubsystem.bindUserPersonalFile(reciever.login, recieverFile))
					return false;

				recieverFile.seekg(0, std::ios_base::end);
				recieverFile << sender.nickname << ' ' << std::move(message) << std::endl;
				recieverFile.close();


				//Отправка сообщения пользователю
				string message_to_client = "\t-> new private message from ";
				message_to_client += sender.nickname;

				if (sendToUser(reciever.login, message_to_client)) {
					std::cout << SERVER_LOG_MESSAGE << "message recieved" << std::endl;
					strcpy(buffer, "true: message recieved");
				} else {
					std::cout << SERVER_LOG_MESSAGE << "message recieved but not delivered" << std::endl;
					strcpy(buffer, "true: message recieved but not delivered");
				}
			}
			break;
		case ALL_MESSAGES:
			std::cout << SERVER_LOG_MESSAGE << "all messages request" << std::endl;
			//Структура запроса должна быть вида: ИДЕНТИФИКАТОР ЗАПРОСА - ДЕСКРИПТОР - ТРЕБУЕМОЕ КОЛ-ВО СООБЩЕНИЙ

			if (bufferSections.size() != 3) {
				std::cerr << SERVER_LOG_MESSAGE << "invalid request format" << std::endl;
				strcpy(buffer, "false: invalid request format");
				return false;
			}

			{
				//Извлечение секций буфера
				string descriptor = getBufferSection(bufferSections, 1);
				string countStr = getBufferSection(bufferSections, 2);

				unsigned int count = stoul(countStr); //Преобразование требуемого кол-ва сообщений из текста в unsigned
				if (count == 0) {
					std::cerr << SERVER_LOG_MESSAGE << "messages count in request not valid" << std::endl;
					strcpy(buffer, "false: messages count in request not valid");
					return false;
				}

				//Если такой дескриптор не выдан - отклонение запроса
				if (!descriptorExist(descriptor))
					return false;

				auto current = messages.end()-1;
				if (messages.size() > count)
					current = messages.begin()+count-1;

				//Формирование сообщения клиенту
				string message_to_client;
				message_to_client += "true: \n----\n";
				while (current >= messages.begin()) {
					message_to_client += "\t";
					message_to_client += current->own;
					message_to_client += ": ";
					message_to_client += current->msg;
					message_to_client += '\n';
					current--;
				}

				//Ресайз очереди общих сообщений при превышении предела, для контроля памяти
				if (messages.size() > MESSAGES_MAXIMUM_STORED) {
					messages.resize(MESSAGES_LOAD_COUNT);
				}

				std::cout << SERVER_LOG_MESSAGE << "global messages transmitted" << std::endl;
				strcpy(buffer, message_to_client.data());
			}
			break;
		case PERSONAL_MESSAGES:
			std::cout << SERVER_LOG_MESSAGE << "personal messages request" << std::endl;
			//Структура запроса должна быть вида: ИДЕНТИФИКАТОР ЗАПРОСА - ДЕСКРИПТОР

			if (bufferSections.size() != 2) {
				std::cerr << SERVER_LOG_MESSAGE << "invalid request format" << std::endl;
				strcpy(buffer, "false: invalid request format");
				return false;
			}

			{
				string descriptor = getBufferSection(bufferSections, 1);

				if (!descriptorExist(descriptor))
					return false;

				const Server::User& user = *(logins.find(issuedDescriptors[descriptor])->second);

				std::fstream userPersonalMessagesFile;

				if(!fileSubsystem.bindUserPersonalFile(user.login, userPersonalMessagesFile))
					return false;

				string input, message_to_client;
				message_to_client += "true: ";
				while (userPersonalMessagesFile >> input) {
					message_to_client += '\t';
					message_to_client += input;
					message_to_client += ": ";

					getline(userPersonalMessagesFile, input);
					message_to_client += input;
					message_to_client += '\n';
				}

				userPersonalMessagesFile.close();
				
				strcpy(buffer, message_to_client.data());
				std::cout << SERVER_LOG_MESSAGE << "personal messages transmitted" << std::endl;
			}
			break;
		case LIST_USERS:
			std::cout << SERVER_LOG_MESSAGE << "users list request" << std::endl;
			//Структура запроса должна быть вида: ИДЕНТИФИКАТОР ЗАПРОСА - ДЕСКРИПТОР

			if (bufferSections.size() != 2) {
				std::cerr << SERVER_LOG_MESSAGE << "invalid request format" << std::endl;
				strcpy(buffer, "false: invalid request format");
				return false;
			}

			{
				string descriptor = getBufferSection(bufferSections, 1);

				if (!descriptorExist(descriptor))
					return false;

				string message_to_client;
				message_to_client += "true: ";
				for (const auto& user : logins) {
					message_to_client += user.second->login;
					message_to_client += '\n';
				}

				strcpy(buffer, message_to_client.data());
				std::cout << SERVER_LOG_MESSAGE << "users list transmitted" << std::endl;
				return true;
			}
			break;
		case MYSELF:
			std::cout << SERVER_LOG_MESSAGE << "client info request" << std::endl;
			//Структура запроса должна быть вида: ИДЕНТИФИКАТОР ЗАПРОСА - ДЕСКРИПТОР

			if (bufferSections.size() != 2) {
				std::cerr << SERVER_LOG_MESSAGE << "invalid request format" << std::endl;
				strcpy(buffer, "false: invalid request format");
				return false;
			}

			{
				string descriptor = getBufferSection(bufferSections, 1);

				if (!descriptorExist(descriptor))
					return false;

				const Server::User& user = *(logins.find(issuedDescriptors[descriptor])->second);

				string message_to_client;
				message_to_client += user.login;
				message_to_client += ' ';
				message_to_client += user.nickname;

				std::cout << SERVER_LOG_MESSAGE << "user info transmitted" << std::endl;
				strcpy(buffer, "true: ");
				strcpy(buffer+6, message_to_client.data());
			}
			break;
		case DISCONNECT:
			std::cout << SERVER_LOG_MESSAGE << "disconnect request" << std::endl;
			//Структура запроса должна быть вида: ИДЕНТИФИКАТОР ЗАПРОСА - ДЕСКРИПТОР

			if (bufferSections.size() != 2) {
				std::cerr << SERVER_LOG_MESSAGE << "invalid request format" << std::endl;
				strcpy(buffer, "false: invalid request format");
				return false;
			}

			{
				string descriptor = getBufferSection(bufferSections, 1);

				if (!descriptorExist(descriptor))
					return false;

				auto user_it = issuedDescriptors.find(descriptor);

				//Удаление пользователя из "авторизованных (пользователей онлайн)"
				loggedClients.erase(loggedClients.find(user_it->second));
				//Удаление выданного дескриптора
				issuedDescriptors.erase(user_it);

				std::cout << SERVER_LOG_MESSAGE << "client disconnected" << std::endl;
				strcpy(buffer, "true: disconnected");
			}
			break;
		default: //Если запрос не опознан
			std::cout << SERVER_LOG_MESSAGE << "request not found" << std::endl;
			return false;
			break;
	}

	std::cout << SERVER_LOG_MESSAGE << "request processing completed" << std::endl;
	return true;
}

//Добавление нового пользователя
//Логин и псевдоним ассоциируется в своих map с определенным адресом созданного пользователя
void Server::addUser(const string& login, const string& pass, const string& nickname)
{
	loadUserToMemory(std::move(login), std::move(pass), std::move(nickname));
	fileSubsystem.usersFile << login << ' ' << pass << ' ' << nickname << std::endl;
}

//Удаление пользователя
//Уничтожаются записи в обоих map, память, выданная на пользователя, освобождается
bool Server::delUser(const string& login)
{
	auto loginIt = logins.find(login);
	if (loginIt == logins.end())
		return false;

	string nick = loginIt->second->nickname;
	auto nicknameIt = nicknames.find(nick);

	delete loginIt->second;

	logins.erase(loginIt);
	nicknames.erase(nicknameIt);

	return true;
}

//Проверка на валидность идентификатора
bool Server::name_check(const string& name, string& result) const
{
	if (name.empty() || name.size() > NAME_MAX_SIZE) {
		result = "Cannnot be empty and more then ";
		result += std::to_string(NAME_MAX_SIZE);

		return false;
	}

	for (char i : name) {
		if (i < '!' || i > '~') {
			result = "Must containe from ! to ~ characters in ASCII table";
			return false;
		}
	}

	return true;
}

//Существует ли логин?
bool Server::loginExist(const string& login) const
{
	return logins.find(login) == logins.end() ? false : true;
}

//Существует ли псевдоним?
bool Server::nicknameExist(const string& nickname) const
{
	return nicknames.find(nickname) == nicknames.end() ? false : true;
}

//Выдан ли дескриптор?
bool Server::descriptorExist(const string& descr) const
{
	if (issuedDescriptors.find(descr) == issuedDescriptors.end()) {
		std::cerr << SERVER_LOG_MESSAGE << "account unavailable" << std::endl;
		strcpy(buffer, "false: account unavailable");
		return false;
	} else
		return true;
}

//Добавление пользователя в память
void Server::loadUserToMemory(const string& login, const string& pass, const string& nickname)
{
	User *newUser		= new User(login, nickname, pass);
	logins[login]		= newUser;
	nicknames[nickname]	= newUser;
}

//Загрузка в память пользователей из файла
bool Server::loadUsersFromFile(std::fstream& file)
{
	while (file) {
		string login, pass, nickname;
		file >> login >> pass >> nickname;

		loadUserToMemory(std::move(login), std::move(pass), std::move(nickname));
	}

	file.clear(); //Сброс флага конца файла
	return true;
}

//Загрузка глобальных сообщений в память из файла
void Server::loadGlobalMessagesFromFile(std::fstream& file, const size_t need_lines)
{
	/*
	 * need_lines - параметр, определяющий какое нужно кол-во строк, считая
	 * с конца файла.
	 * Сначала идем в конец файла, определяем позицию. Если позиция нулевая,
	 * значит файл пустой, возвращаем управление.
	 * Заводим счетчик кол-ва обнаруженных строк.
	 * Пока рассматриваемая позиция не дошла до начала файла или не набралось
	 * количество необходимых строк, идем по файлу назад.
	 * Встаем на начало нужной строки. Считываем файл.
	 */

	file.seekg(0, std::ios_base::end);
	size_t cur_pos = file.tellg();

	if (cur_pos == 0)
		return;

	size_t lines_count = 0;

	while (cur_pos != 0) {
		int c = file.peek();

		if (c == '\n')
			lines_count++;

		if (lines_count == need_lines+1) {
			file.seekg(cur_pos+1);
			break;
		}

		file.seekg(--cur_pos);
	}

	std::string own, msg;

	while (file >> own) {
		getline(file, msg);
		messages.emplace_front(std::move(own), std::move(msg));
	}

	file.clear();
}

bool Server::sendToUser(const string& login, const string& message)
{
	if (loggedClients.find(login) != loggedClients.end()) {
		sockaddr_in &other_user = loggedClients[login];
		int socket_to_client = socket(AF_INET, SOCK_STREAM, 0);
		if (socket_to_client == -1) {
			printf("%scannot create socket to user [%s] to send message\n", SERVER_LOG_MESSAGE, login.data());
			return false;
		}

		int status = connect(socket_to_client, (sockaddr*) &other_user, sizeof(other_user));
		if (status == -1) {
			printf("%scannot connect to user [%s] to send message\n", SERVER_LOG_MESSAGE, login.data());
			loggedClients.erase(loggedClients.find(login));
			close(socket_to_client);
			return false;
		} else {
			unsigned count = write(socket_to_client, message.data(), message.size());
			if (count != message.size()) {
				printf("%scannot send message to user [%s]\n", SERVER_LOG_MESSAGE, login.data());
				close(socket_to_client);
				return false;
			} else {
				printf("%smessage to [%s] sended\n", SERVER_LOG_MESSAGE, login.data());
				return true;
			}
		}
		close(socket_to_client);
	} else {
			printf("%scannot send message to user [%s]\n", SERVER_LOG_MESSAGE, login.data());
			return false;
	}
}
