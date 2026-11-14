#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <sstream>
#include "doctest.h"
#include "wire/data_item.hpp"

TEST_CASE("Data item parsing")
{
	std::istringstream input_stream;

	SUBCASE("Parsing an atom")
	{
		input_stream.str("ATOM x");
		climap::wire::response_stream<std::istringstream> stream(input_stream);
		climap::wire::data_item item;
		stream >> item;
		CHECK(item.data_type() == climap::wire::data_item::atom);
		CHECK(std::get<std::string>(item.data_value()) == "ATOM");
	}

	SUBCASE("Parsing an astring")
	{
		input_stream.str("ATO[]M x");
		climap::wire::response_stream<std::istringstream> stream(input_stream);
		climap::wire::data_item item;
		stream >> item;
		CHECK(item.data_type() == climap::wire::data_item::astring);
		CHECK(std::get<std::string>(item.data_value()) == "ATO[]M");
	}

	SUBCASE("Parsing a quoted string")
	{
		input_stream.str("\"Quoted String\"\r\n");
		climap::wire::response_stream<std::istringstream> stream(input_stream);
		climap::wire::data_item item;
		stream >> item;
		CHECK(item.data_type() == climap::wire::data_item::quoted_string);
		CHECK(std::get<std::string>(item.data_value()) == "Quoted String");
	}

	SUBCASE("Parsing a literal")
	{
		input_stream.str("{5}\r\nHello ");
		climap::wire::response_stream<std::istringstream> stream(input_stream);
		climap::wire::data_item item;
		stream >> item;
		CHECK(item.data_type() == climap::wire::data_item::literal);
		CHECK(std::get<std::string>(item.data_value()) == "Hello");
	}

	SUBCASE("Parsing a numeric value")
	{
		input_stream.str("1234\r\n");
		climap::wire::response_stream<std::istringstream> stream(input_stream);
		climap::wire::data_item item;
		stream >> item;
		CHECK(item.data_type() == climap::wire::data_item::num);
		CHECK(std::get<unsigned>(item.data_value()) == 1234);
	}

	SUBCASE("Parsing a NIL value")
	{
		input_stream.str("NIL ");
		climap::wire::response_stream<std::istringstream> stream(input_stream);
		climap::wire::data_item item;
		stream >> item;
		CHECK(item.data_type() == climap::wire::data_item::nil);
	}

	SUBCASE("Parsing a list")
	{
		input_stream.str("(ATOM \"String\" 123 NIL (first second)) ");
		climap::wire::response_stream<std::istringstream> stream(input_stream);
		climap::wire::data_item item;
		stream >> item;
		CHECK(item.data_type() == climap::wire::data_item::list);
		CHECK(std::get<std::vector<climap::wire::data_item>>(item.data_value()).size() == 5);
	}

	SUBCASE("Parsing incorrect input")
	{
		// Test parsing an incomplete quoted string
		// input_stream.str("\"Incomplete Quoted String");
		// climap::wire::response_stream<std::istringstream> stream1(input_stream);
		// climap::wire::data_item item1;
		// CHECK_THROWS(stream1 >> item1);

		// Test parsing a literal with missing delimiter '}'
		input_stream.str("{5\r\nHello");
		climap::wire::response_stream<std::istringstream> stream2(input_stream);
		climap::wire::data_item item2;
		CHECK_THROWS(stream2 >> item2);

		// Test parsing a literal with missing CR
		input_stream.str("{5}\nHello");
		climap::wire::response_stream<std::istringstream> stream3(input_stream);
		climap::wire::data_item item3;
		CHECK_THROWS(stream3 >> item3);

		// Test parsing a literal with missing LF
		input_stream.str("{5}\rHello");
		climap::wire::response_stream<std::istringstream> stream4(input_stream);
		climap::wire::data_item item4;
		CHECK_THROWS(stream4 >> item4);
	}
}
