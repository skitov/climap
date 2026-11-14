#pragma once

#include <string>
#include <vector>
#include <cctype>
#include <functional>
#include <variant>

#include "response_stream.hpp"
#include "parser_exception.hpp"
#include "lib/util.hpp"

namespace climap::wire {

class data_item
{
	friend class response;
public:
	data_item(){};
	enum type
	{
		astring = 0x1,
		quoted_string = 0x2,
		literal = 0x4,
		num = 0x8,
		list = 0x10,
		nil = 0x20,
		atom = 0x41
	};

	template<typename istream>
	friend response_stream<istream> &operator >>(response_stream<istream> &i, data_item &d)
	{
		char c;
		// Read first char to figure out what type of item to read
		i >> c;
		auto read_item = [&c, &i, &d](std::function<bool(void)> read_typed)
		{
			if (read_typed())
			{
				// End of item was clear without reading item delimiter
				i >> c;
			}
			i.data_item_ended_with(c);
		};
		switch (c)
		{
		case '"':
			// Quoted string item
			read_item([&c, &i, &d]() -> bool
			{
				d.item_type = type::quoted_string;
				d.value = std::string();
				i >> c;
				while (c != '"')
				{
					if (c == '\\')
						i >> c;
					if (c != '\0' && c < 0x80)
						std::get<std::string>(d.value) += c;
					else
						throw parser_exception("8-bit character in quoted string.");
					i >> c;
				}
				return true;
			});
			break;
		case '(':
			// List item
			read_item([&c, &i, &d]() -> bool
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
			});
			break;
		case '{':
			// Literal item
			read_item([&c, &i, &d]() -> bool
			{
				d.item_type = type::literal;
				d.value = std::string();
				unsigned int data_len = 0;
				i >> c;
				while (std::isdigit(c))
				{
					data_len += data_len*10 + c - '0';
					i >> c;
				}
				if (c != '}')
					throw parser_exception("Missing '}' after number of bytes in literal");
				i >> c;
				if (c != '\r')
					throw parser_exception("Missing CR after number of bytes in literal");
				i >> c;
				if (c != '\n')
					throw parser_exception("Missing LF after number of bytes in literal");
				std::get<std::string>(d.value).resize(data_len);
				i.stream.read(&std::get<std::string>(d.value)[0], data_len);
				return true;
			});
			break;
		default:
			// This can be atom/astring, nil or number
			read_item([&c, &i, &d]() -> bool
			{
				bool maybe_num = true;
				d.value = std::string();
				while (!is_astring_special(c))
				{
					if (is_resp_special(c))
					{
						maybe_num = false;
						d.item_type = astring;
					}
					if (maybe_num && !std::isdigit(c))
						maybe_num = false;
					std::get<std::string>(d.value) += c;
					i >> c;
				}
				if (maybe_num)
				{
					d.item_type = num;
					d.value = (unsigned)std::stoi(std::get<std::string>(d.value));
					return false;
				}
				if (d.item_type == astring)
					return false;
				if (lib::str_eq_ignore_case(std::get<std::string>(d.value), "nil"))
					d.item_type = nil;
				else
					d.item_type = atom;
				return false;
			});
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
	static bool is_astring_special(char c)
	{
		return is_ctl(c) ||
			is_quoted_special(c) ||
			is_list_wildcard(c) ||
			c == '(' || c == ')' || c == '{' || c == ' ';
	}
	static bool is_atom_special(char c)
	{
		return is_resp_special(c) || is_astring_special(c);
	}
	std::variant<unsigned, std::string, std::vector<data_item>> value;
	type item_type;
};

};
