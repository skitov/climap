#pragma once

#include <exception>
#include <string>


namespace climap::wire
{
class parser_exception : std::exception
{
public:
	parser_exception(std::string &msg) : message(msg){}
	parser_exception(const char *msg) : message(msg){}
	virtual char *what() {return message.data();}
private:
	std::string message;
};
};
