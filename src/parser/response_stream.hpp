#pragma once
#include "parser_exception.hpp"

namespace imap_parser
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
	response_stream(istream &underlying_stream) : stream(underlying_stream) {}
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
			stream.read(&c, 1);
			if (c != '\n')
				throw parser_exception("Expected LF after CR");
			last_item_end = crlf;
			break;
		default:
			throw parser_exception("Data item ended with unexcpected character.");
		}
	}
	istream &stream;
	bool new_response;
	unsigned int list_level;
	data_item_end last_item_end;
};

};
