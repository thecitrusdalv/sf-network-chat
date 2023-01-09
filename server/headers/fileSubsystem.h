//fileSubsystem.h
#pragma once

#include <fstream>
#include <string>

#define FS_LOG_MESSAGE "-> File subsystem: " //Заголовок информационного сообщения

class FileSubsystem
{
public:
	FileSubsystem() = default;
	~FileSubsystem() = default;

	std::fstream globalMessagesFile; //Файловый поток глобальных сообщений
	std::fstream usersFile; //Файловый поток со сведениями зарегистрированных пользователей

	bool init(); //Инициализация файловой подсистемы

	//Связывание переданного потока с нужным файлом пользователя
	bool bindUserPersonalFile (const std::string&, std::fstream&);

private: 
	bool open_file(std::fstream&, const std::string&); //Открытие(создание) файла подсистемой
	void set_permissions(const std::string&); //Выставление прав на файл
};
