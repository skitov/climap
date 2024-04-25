#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <sstream>
#include "doctest.h"
#include "parser/data_item.hpp"

// TEST_CASE("Special symbols")
// {

// }

TEST_CASE("Data item parsing")
{
	std::istringstream input_stream;

	SUBCASE("Parsing an atom")
	{
		input_stream.str("ATOM x");
		imap_parser::response_stream<std::istringstream> stream(input_stream);
		imap_parser::data_item item;
		stream >> item;
		CHECK(item.data_type() == imap_parser::data_item::atom);
		CHECK(std::get<std::string>(item.data_value()) == "ATOM");
	}

	SUBCASE("Parsing a quoted string")
	{
		input_stream.str("\"Quoted String\"\r\n");
		imap_parser::response_stream<std::istringstream> stream(input_stream);
		imap_parser::data_item item;
		stream >> item;
		CHECK(item.data_type() == imap_parser::data_item::quoted_string);
		CHECK(std::get<std::string>(item.data_value()) == "Quoted String");
	}

	SUBCASE("Parsing a literal")
	{
		input_stream.str("{5}\r\nHello ");
		imap_parser::response_stream<std::istringstream> stream(input_stream);
		imap_parser::data_item item;
		stream >> item;
		CHECK(item.data_type() == imap_parser::data_item::literal);
		CHECK(std::get<std::string>(item.data_value()) == "Hello");
	}

	SUBCASE("Parsing a numeric value")
	{
		input_stream.str("1234\r\n");
		imap_parser::response_stream<std::istringstream> stream(input_stream);
		imap_parser::data_item item;
		stream >> item;
		CHECK(item.data_type() == imap_parser::data_item::num);
		CHECK(std::get<unsigned>(item.data_value()) == 1234);
	}

	SUBCASE("Parsing a NIL value")
	{
		input_stream.str("NIL ");
		imap_parser::response_stream<std::istringstream> stream(input_stream);
		imap_parser::data_item item;
		stream >> item;
		CHECK(item.data_type() == imap_parser::data_item::nil);
	}

	SUBCASE("Parsing a list")
	{
		input_stream.str("(ATOM \"String\" 123 NIL (first second)) ");
		imap_parser::response_stream<std::istringstream> stream(input_stream);
		imap_parser::data_item item;
		stream >> item;
		CHECK(item.data_type() == imap_parser::data_item::list);
		CHECK(std::get<std::vector<imap_parser::data_item>>(item.data_value()).size() == 5);
	}

	SUBCASE("Parsing incorrect input")
	{
		// Test parsing an incomplete quoted string
		// input_stream.str("\"Incomplete Quoted String");
		// imap_parser::response_stream<std::istringstream> stream1(input_stream);
		// imap_parser::data_item item1;
		// CHECK_THROWS(stream1 >> item1);

		// Test parsing a literal with missing delimiter '}'
		input_stream.str("{5\r\nHello");
		imap_parser::response_stream<std::istringstream> stream2(input_stream);
		imap_parser::data_item item2;
		CHECK_THROWS(stream2 >> item2);

		// Test parsing a literal with missing CR
		input_stream.str("{5}\nHello");
		imap_parser::response_stream<std::istringstream> stream3(input_stream);
		imap_parser::data_item item3;
		CHECK_THROWS(stream3 >> item3);

		// Test parsing a literal with missing LF
		input_stream.str("{5}\rHello");
		imap_parser::response_stream<std::istringstream> stream4(input_stream);
		imap_parser::data_item item4;
		CHECK_THROWS(stream4 >> item4);
	}
}
