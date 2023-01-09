//user.cpp

#include "../headers/server.h"

		Server::User::User(const std::string& _login, const std::string& _nick, const std::string& _pass) :
			login(_login), nickname(_nick), pass(_pass)
		{}

		Server::User::User(const User&) = delete;

		Server::User::User(User&& other)
		{
			login = std::move(other.login);
			nickname = std::move(other.nickname);
			pass = std::move(other.pass);
		}

		Server::User& Server::User::operator= (const User&) = delete;

		Server::User& Server::User::operator= (User&& other)
		{
			login = std::move(other.login);
			nickname = std::move(other.nickname);
			pass = std::move(other.pass);
			return *this;
		}

		Server::User::~User() = default;
