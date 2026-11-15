#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "doctest.h"
#include "wire/data_item.hpp"
#include "wire/reader.hpp"

using climap::wire::data_item;
using climap::wire::item_kind;
using climap::wire::limits;
using climap::wire::parse_error;
using climap::wire::parse_error_kind;
using climap::wire::reader;

namespace
{

template <typename Fn>
std::optional<parse_error> capture_error(Fn fn)
{
	try
	{
		fn();
	}
	catch (const parse_error &e)
	{
		return e;
	}
	return std::nullopt;
}

data_item parse(const std::string &text)
{
	std::istringstream in(text);
	reader r(*in.rdbuf());
	return climap::wire::read_item(r);
}

}

TEST_CASE("Data item parsing")
{
	SUBCASE("Parsing an atom")
	{
		std::istringstream in("ATOM x");
		reader r(*in.rdbuf());

		const data_item item = climap::wire::read_item(r);
		CHECK(item.data_type() == item_kind::atom);
		CHECK(std::get<std::string>(item.data_value()) == "ATOM");
		CHECK(r.peek() == ' ');
	}

	SUBCASE("Parsing an astring")
	{
		const data_item item = parse("ATO[]M x");
		CHECK(item.data_type() == item_kind::astring);
		CHECK(std::get<std::string>(item.data_value()) == "ATO[]M");
	}

	SUBCASE("Parsing a quoted string")
	{
		const data_item item = parse("\"Quoted String\"\r\n");
		CHECK(item.data_type() == item_kind::quoted);
		CHECK(std::get<std::string>(item.data_value()) == "Quoted String");
	}

	SUBCASE("Quoted strings accept 8-bit bytes")
	{
		std::string input = "\"";
		input.push_back(static_cast<char>(0x80));
		input += "\" ";

		const data_item item = parse(input);
		CHECK(item.data_type() == item_kind::quoted);
		CHECK(std::get<std::string>(item.data_value()) ==
		      std::string(1, static_cast<char>(0x80)));
	}

	SUBCASE("Quoted strings support escapes")
	{
		const data_item item = parse("\"a\\\"b\\\\c\" ");
		CHECK(std::get<std::string>(item.data_value()) == "a\"b\\c");
	}

	SUBCASE("Parsing a literal")
	{
		const data_item item = parse("{5}\r\nHello ");
		CHECK(item.data_type() == item_kind::literal);
		CHECK(std::get<std::string>(item.data_value()) == "Hello");
	}

	SUBCASE("Literal length is accumulated correctly")
	{
		const std::string payload = "Hello, World";
		const data_item item = parse("{12}\r\n" + payload);
		CHECK(item.data_type() == item_kind::literal);
		CHECK(std::get<std::string>(item.data_value()) == payload);
	}

	SUBCASE("Empty literals are allowed")
	{
		const data_item item = parse("{0}\r\n ");
		CHECK(item.data_type() == item_kind::literal);
		CHECK(std::get<std::string>(item.data_value()).empty());
	}

	SUBCASE("Parsing a numeric value")
	{
		const data_item item = parse("1234\r\n");
		CHECK(item.data_type() == item_kind::number);
		CHECK(std::get<unsigned>(item.data_value()) == 1234u);
	}

	SUBCASE("Numeric prefix does not make an atom a number")
	{
		const data_item item = parse("123abc ");
		CHECK(item.data_type() == item_kind::atom);
		CHECK(std::get<std::string>(item.data_value()) == "123abc");
	}

	SUBCASE("Parsing a NIL value")
	{
		const data_item item = parse("NIL ");
		CHECK(item.data_type() == item_kind::nil);
		CHECK(std::holds_alternative<std::monostate>(item.data_value()));
	}

	SUBCASE("NIL is case-insensitive")
	{
		CHECK(parse("nil ").data_type() == item_kind::nil);
		CHECK(parse("Nil ").data_type() == item_kind::nil);
	}

	SUBCASE("NIL with a response special is an astring")
	{
		const data_item item = parse("NIL] ");
		CHECK(item.data_type() == item_kind::astring);
		CHECK(std::get<std::string>(item.data_value()) == "NIL]");
	}

	SUBCASE("Parsing a list")
	{
		const data_item item = parse("(ATOM \"String\" 123 NIL (first second)) ");
		REQUIRE(item.data_type() == item_kind::list);

		auto &items = std::get<std::vector<data_item>>(item.data_value());
		REQUIRE(items.size() == 5);
		CHECK(items[0].data_type() == item_kind::atom);
		CHECK(items[1].data_type() == item_kind::quoted);
		CHECK(items[2].data_type() == item_kind::number);
		CHECK(items[3].data_type() == item_kind::nil);
		REQUIRE(items[4].data_type() == item_kind::list);
		CHECK(std::get<std::vector<data_item>>(items[4].data_value()).size() == 2);
	}

	SUBCASE("Parsing an empty list")
	{
		const data_item item = parse("() ");
		REQUIRE(item.data_type() == item_kind::list);
		CHECK(std::get<std::vector<data_item>>(item.data_value()).empty());
	}

	SUBCASE("Data items compare by value")
	{
		CHECK(parse("ATOM ") == parse("ATOM "));
		CHECK(parse("NIL ") == parse("nil "));
		CHECK(parse("ATOM ") != parse("ATOMX "));
	}

	SUBCASE("Unterminated quoted string")
	{
		std::istringstream in("\"Incomplete");
		reader r(*in.rdbuf());

		auto err = capture_error([&] { climap::wire::read_item(r); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == parse_error_kind::unexpected_eof);
	}

	SUBCASE("Invalid escape in quoted string")
	{
		std::istringstream in("\"a\\q\" ");
		reader r(*in.rdbuf());

		auto err = capture_error([&] { climap::wire::read_item(r); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == parse_error_kind::invalid_character);
		CHECK(err->offset() == 3);
	}

	SUBCASE("CR or LF inside a quoted string")
	{
		std::istringstream in("\"a\rb\" ");
		reader r(*in.rdbuf());

		auto err = capture_error([&] { climap::wire::read_item(r); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == parse_error_kind::invalid_character);
	}

	SUBCASE("Literal with a missing closing brace")
	{
		std::istringstream in("{5\r\nHello");
		reader r(*in.rdbuf());

		auto err = capture_error([&] { climap::wire::read_item(r); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == parse_error_kind::invalid_character);
	}

	SUBCASE("Literal with a missing CR")
	{
		std::istringstream in("{5}\nHello");
		reader r(*in.rdbuf());

		auto err = capture_error([&] { climap::wire::read_item(r); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == parse_error_kind::bad_crlf);
	}

	SUBCASE("Literal with a missing LF")
	{
		std::istringstream in("{5}\rHello");
		reader r(*in.rdbuf());

		auto err = capture_error([&] { climap::wire::read_item(r); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == parse_error_kind::bad_crlf);
	}

	SUBCASE("Literal shorter than its declared length")
	{
		std::istringstream in("{5}\r\nHell");
		reader r(*in.rdbuf());

		auto err = capture_error([&] { climap::wire::read_item(r); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == parse_error_kind::unexpected_eof);
	}

	SUBCASE("Number overflow")
	{
		std::istringstream in("4294967296 ");
		reader r(*in.rdbuf());

		auto err = capture_error([&] { climap::wire::read_item(r); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == parse_error_kind::number_overflow);
	}

	SUBCASE("Unterminated list")
	{
		std::istringstream in("(ATOM");
		reader r(*in.rdbuf());

		auto err = capture_error([&] { climap::wire::read_item(r); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == parse_error_kind::unexpected_eof);
	}

	SUBCASE("Trailing space in a list")
	{
		std::istringstream in("(ATOM )");
		reader r(*in.rdbuf());

		auto err = capture_error([&] { climap::wire::read_item(r); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == parse_error_kind::invalid_character);
	}

	SUBCASE("Literal size limit")
	{
		limits lim;
		lim.max_literal_bytes = 4;

		std::istringstream in("{5}\r\nHello ");
		reader r(*in.rdbuf(), lim);

		auto err = capture_error([&] { climap::wire::read_item(r); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == parse_error_kind::limit_exceeded);
	}

	SUBCASE("List depth limit")
	{
		limits lim;
		lim.max_list_depth = 2;

		std::istringstream in("((())) ");
		reader r(*in.rdbuf(), lim);

		auto err = capture_error([&] { climap::wire::read_item(r); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == parse_error_kind::limit_exceeded);
	}

	SUBCASE("Atom size limit")
	{
		limits lim;
		lim.max_atom_bytes = 3;

		std::istringstream in("ABCD ");
		reader r(*in.rdbuf(), lim);

		auto err = capture_error([&] { climap::wire::read_item(r); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == parse_error_kind::limit_exceeded);
	}

	SUBCASE("Quoted string size limit")
	{
		limits lim;
		lim.max_quoted_bytes = 4;

		std::istringstream in("\"abcde\" ");
		reader r(*in.rdbuf(), lim);

		auto err = capture_error([&] { climap::wire::read_item(r); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == parse_error_kind::limit_exceeded);
	}
}
