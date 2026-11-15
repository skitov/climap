#pragma once

#include <cstddef>
#include <cstdint>
#include <cctype>
#include <limits>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "lib/util.hpp"
#include "limits.hpp"
#include "parse_error.hpp"
#include "reader.hpp"

namespace climap::wire
{

enum class item_kind
{
	undefined,
	atom,
	astring,
	quoted,
	literal,
	number,
	nil,
	list,
};

// Generic, lossless representation of one IMAP data item.
//
// Parsing only enforces the wire grammar; response semantics are applied by
// command-aware decoders in a higher layer.
class data_item
{
public:
	using value_type =
		std::variant<std::monostate, unsigned, std::string, std::vector<data_item>>;

	data_item() = default;
	data_item(item_kind kind, value_type value)
		: kind_(kind), value_(std::move(value))
	{
	}

	item_kind data_type() const noexcept { return kind_; }

	value_type &data_value() noexcept { return value_; }
	const value_type &data_value() const noexcept { return value_; }

	bool operator==(const data_item &) const = default;

private:
	item_kind kind_ = item_kind::undefined;
	value_type value_;
};

namespace detail
{

inline bool is_astring_special(unsigned char c)
{
	return c <= 0x1f || c == 0x7f ||
	       c == '"' || c == '\\' ||
	       c == '%' || c == '*' ||
	       c == '(' || c == ')' || c == '{' || c == ' ';
}

inline data_item read_item(reader &r, const limits &lim, std::size_t depth);

inline data_item read_quoted(reader &r, const limits &lim)
{
	r.expect('"');

	std::string text;
	while (true)
	{
		const std::size_t at = r.offset();
		int c = r.get();
		if (c == '"')
			break;

		if (c == '\\')
		{
			const std::size_t escaped_at = r.offset();
			c = r.get();
			if (c != '"' && c != '\\')
				throw parse_error(parse_error_kind::invalid_character, escaped_at,
				                  "invalid escape in quoted string");
		}
		else if (c == '\r' || c == '\n')
		{
			throw parse_error(parse_error_kind::invalid_character, at,
			                  "CR or LF in quoted string");
		}

		if (text.size() >= lim.max_quoted_bytes)
			throw parse_error(parse_error_kind::limit_exceeded, at,
			                  "quoted string exceeds max_quoted_bytes (" +
			                      std::to_string(lim.max_quoted_bytes) + ")");
		text.push_back(static_cast<char>(c));
	}

	return data_item(item_kind::quoted, std::move(text));
}

inline data_item read_literal(reader &r, const limits &lim)
{
	r.expect('{');

	std::uint64_t length = 0;
	std::size_t digits = 0;
	while (true)
	{
		const int c = r.peek();
		if (c == std::char_traits<char>::eof() ||
		    !std::isdigit(static_cast<unsigned char>(c)))
			break;

		const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
		if (length >
		    (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
			throw parse_error(parse_error_kind::number_overflow, r.offset(),
			                  "literal length overflows 64 bits");

		length = length * 10 + digit;
		++digits;
		r.get();

		if (length > lim.max_literal_bytes)
			throw parse_error(parse_error_kind::limit_exceeded, r.offset(),
			                  "literal exceeds max_literal_bytes (" +
			                      std::to_string(lim.max_literal_bytes) + ")");
	}

	if (digits == 0)
		throw parse_error(parse_error_kind::invalid_character, r.offset(),
		                  "missing literal length");

	r.expect('}');
	r.expect_crlf();

	std::string data(static_cast<std::size_t>(length), '\0');
	r.read_exact(data.data(), data.size());
	return data_item(item_kind::literal, std::move(data));
}

inline data_item read_token(reader &r, const limits &lim)
{
	const std::size_t start = r.offset();
	std::string text;
	bool astring = false;
	bool maybe_number = true;

	while (!r.eof())
	{
		const int c = r.peek();
		const unsigned char uc = static_cast<unsigned char>(c);
		if (is_astring_special(uc))
			break;

		if (uc == ']')
			astring = true;
		if (maybe_number && !std::isdigit(uc))
			maybe_number = false;

		if (text.size() >= lim.max_atom_bytes)
			throw parse_error(parse_error_kind::limit_exceeded, r.offset(),
			                  "atom exceeds max_atom_bytes (" +
			                      std::to_string(lim.max_atom_bytes) + ")");

		text.push_back(static_cast<char>(r.get()));
	}

	if (text.empty())
		throw parse_error(parse_error_kind::invalid_character, start,
		                  "expected a data item");

	if (maybe_number)
	{
		unsigned value = 0;
		for (std::size_t i = 0; i < text.size(); ++i)
		{
			const unsigned digit = static_cast<unsigned>(text[i] - '0');
			if (value > (std::numeric_limits<unsigned>::max() - digit) / 10)
				throw parse_error(parse_error_kind::number_overflow, start + i,
				                  "number does not fit in 32 bits");
			value = value * 10 + digit;
		}
		return data_item(item_kind::number, value);
	}

	if (!astring && lib::str_eq_ignore_case(text, "nil"))
		return data_item(item_kind::nil, std::monostate{});

	return data_item(astring ? item_kind::astring : item_kind::atom,
	                 std::move(text));
}

inline data_item read_list(reader &r, const limits &lim, std::size_t depth)
{
	r.expect('(');

	std::vector<data_item> items;
	if (r.peek() == ')')
	{
		r.get();
		return data_item(item_kind::list, std::move(items));
	}

	while (true)
	{
		items.push_back(read_item(r, lim, depth));

		const std::size_t at = r.offset();
		const int c = r.get();
		if (c == ')')
			break;
		if (c != ' ')
			throw parse_error(parse_error_kind::invalid_character, at,
			                  "expected space or ')' after list item");
	}

	return data_item(item_kind::list, std::move(items));
}

inline data_item read_item(reader &r, const limits &lim, std::size_t depth)
{
	if (r.eof())
		throw parse_error(parse_error_kind::unexpected_eof, r.offset(),
		                  "unexpected end of input before data item");

	switch (r.peek())
	{
	case '"':
		return read_quoted(r, lim);
	case '{':
		return read_literal(r, lim);
	case '(':
		if (depth >= lim.max_list_depth)
			throw parse_error(parse_error_kind::limit_exceeded, r.offset(),
			                  "list nesting exceeds max_list_depth (" +
			                      std::to_string(lim.max_list_depth) + ")");
		return read_list(r, lim, depth + 1);
	default:
		return read_token(r, lim);
	}
}

}

// Reads one data item, leaving the terminating delimiter unconsumed.
inline data_item read_item(reader &r)
{
	return detail::read_item(r, r.limit_config(), 0);
}

}
