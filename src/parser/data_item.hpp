#pragma once

#include <string>
#include <vector>
#include <cctype>
#include <functional>
#include <variant>

#include "response_stream.hpp"
#include "parser_exception.hpp"

namespace imap_parser {

class data_item
{
public:
	data_item(){};
	enum type
	{
		atom = 0x1,
		quoted_string = 0x2,
		literal = 0x4,
		num = 0x8,
		list = 0x10,
		nil = 0x20
	};

	template<typename istream>
	friend response_stream<istream> &operator >>(response_stream<istream> &i, data_item &d)
	{
		char c;
		i.stream.read(&c, 1);
		auto read_item = [&c, &i, &d](std::function<bool(void)> read_typed)
		{
			if (read_typed())
				i.stream.read(&c, 1);
			i.data_item_ended_with(c);
		};
		auto read_quoted = [&c, &i, &d]() -> bool
		{
			d.item_type = type::quoted_string;
			d.value = std::string();
			i.stream.read(&c, 1);
			while (c != '"')
			{
				if (c == '\\')
					i.stream.read(&c, 1);
				if (c != '\0' && c < 0x80)
					std::get<std::string>(d.value) += c;
				else
					throw parser_exception("8-bit character in quoted string.");
				i.stream.read(&c, 1);
			}
			return true;
		};
		auto read_list = [&c, &i, &d]() -> bool
		{
			d.item_type = type::list;
			d.value = std::vector<data_item>();
			++i.list_level;
			do
			{
				data_item list_entry;
				i >> list_entry;
				std::get<std::vector<data_item>>(d.value).push_back(list_entry);
			} while (i.last_item_end != response_stream<istream>::list_end);
			--i.list_level;
			return true;
		};
		auto read_literal = [&c, &i, &d]() -> bool
		{
			d.item_type = type::literal;
			d.value = std::string();
			unsigned int data_len = 0;
			i.stream.read(&c, 1);
			while (std::isdigit(c))
			{
				data_len += data_len*10 + c - '0';
				i.stream.read(&c, 1);
			}
			if (c != '}')
				throw parser_exception("Missing '}' after number of bytes in literal");
			i.stream.read(&c, 1);
			if (c != '\r')
				throw parser_exception("Missing CR after number of bytes in literal");
			i.stream.read(&c, 1);
			if (c != '\n')
				throw parser_exception("Missing LF after number of bytes in literal");
			std::get<std::string>(d.value).resize(data_len);
			i.stream.read(&std::get<std::string>(d.value)[0], data_len);
			return true;
		};
		auto read_atom_num_nil = [&c, &i, &d]() -> bool
		{
			bool maybe_num = true;
			d.value = std::string();
			while (!is_atom_special(c))
			{
				if (maybe_num && !std::isdigit(c))
					maybe_num = false;
				std::get<std::string>(d.value) += c;
				i.stream.read(&c, 1);
			}
			if (maybe_num)
			{
				d.item_type = num;
				d.value = (unsigned)std::stoi(std::get<std::string>(d.value));
			}
			else if (std::get<std::string>(d.value).compare("nil") == 0 ||
				 std::get<std::string>(d.value).compare("NIL") == 0)
				d.item_type = nil;
			else
				d.item_type = atom;
			return false;
		};
		switch (c)
		{
		case '"':
			read_item(read_quoted);
			break;
		case '(':
			read_item(read_list);
			break;
		case '{':
			read_item(read_literal);
			break;
		default:
			read_item(read_atom_num_nil);
			break;
		}
		return i;
	}

	type data_type() {return item_type;}
	std::variant<unsigned, std::string, std::vector<data_item>> &data_value() {return value;}
private:
	static bool is_resp_special(char c)
	{
		return c == ']';
	}
	static bool is_quoted_special(char c)
	{
		return c == '"' || c == '\\';
	}
	static bool is_list_wildcard(char c)
	{
		return c == '%' || c == '*';
	}
	static bool is_ctl(char c)
	{
		return c <= 0x1f || c == 0x7f;
	}
	static bool is_atom_special(char c)
	{
		return is_ctl(c) ||
			is_resp_special(c) ||
			is_quoted_special(c) ||
			is_list_wildcard(c) ||
			c == '(' || c == ')' || c == '{' || c == ' ';
	}
	std::variant<unsigned, std::string, std::vector<data_item>> value;
	type item_type;
};

};
