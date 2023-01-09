//message.cpp

#include "../headers/server.h"

		Server::Message::Message() = default;

		Server::Message::Message(const std::string& _own, const std::string& _msg) :
			own(_own), msg(_msg)
		{}

		Server::Message::Message(const Message& other) = delete;
		Server::Message& Server::Message::operator= (const Message& other) = delete;

		Server::Message::Message(Message&& other)
		{
			if (this != &other)
				*this = std::move(other);
		}


		Server::Message& Server::Message::operator= (Message&& other)
		{
			if (this != &other)
				*this = std::move(other);

			return *this;
		}

		Server::Message::~Message() = default;
