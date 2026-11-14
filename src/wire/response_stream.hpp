#pragma once
#include "parser_exception.hpp"

namespace climap::wire
{

template <typename istream>
class response_stream
{
public:
	enum data_item_end
	{
		undef,
		space,
		list_end,
		crlf
	};
	response_stream(istream &underlying_stream) : stream(underlying_stream), new_response(true) {}
	friend
	response_stream<istream> &operator>>(response_stream<istream> &i, char &c)
	{
		i.stream.read(&c, 1);
		return i;
	}
	void data_item_ended_with(char c)
	{
		switch (c)
		{
		case ' ':
			last_item_end = space;
			break;
		case ')':
			last_item_end = list_end;
			break;
		case '\r':
			*this >> c;
			if (c != '\n')
				throw parser_exception("Expected LF after CR");
			last_item_end = crlf;
			break;
		default:
			throw parser_exception("Data item ended with unexcpected character.");
		}
		new_response = last_item_end == crlf;
	}
	istream &stream;
	bool new_response;
	unsigned int list_level;
	data_item_end last_item_end;
};

};
