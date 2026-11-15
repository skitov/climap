#include <optional>
#include <sstream>
#include <streambuf>
#include <string>

#include "doctest.h"
#include "wire/reader.hpp"

namespace
{

template <typename Fn>
std::optional<climap::wire::parse_error> capture_error(Fn fn)
{
	try
	{
		fn();
	}
	catch (const climap::wire::parse_error &e)
	{
		return e;
	}
	return std::nullopt;
}

// A streambuf that never returns more than two bytes per read, to make sure
// read_exact loops instead of treating a short read as end of input.
class short_read_streambuf : public std::streambuf
{
public:
	explicit short_read_streambuf(std::string data) : data_(std::move(data))
	{
		if (data_.empty())
			setg(nullptr, nullptr, nullptr);
		else
			setg(&data_[0], &data_[0], &data_[0] + data_.size());
	}

protected:
	std::streamsize xsgetn(char *dst, std::streamsize count) override
	{
		return std::streambuf::xsgetn(dst, count < 2 ? count : 2);
	}

private:
	std::string data_;
};

}

TEST_CASE("Wire reader")
{
	SUBCASE("peek, get and offset")
	{
		std::istringstream in("abc");
		climap::wire::reader r(*in.rdbuf());

		CHECK(r.peek() == 'a');
		CHECK(r.offset() == 0);
		CHECK(r.get() == 'a');
		CHECK(r.offset() == 1);
		CHECK(r.get() == 'b');
		CHECK(r.get() == 'c');
		CHECK(r.eof());
		CHECK(r.offset() == 3);
	}

	SUBCASE("get at end of input")
	{
		std::istringstream in("");
		climap::wire::reader r(*in.rdbuf());

		CHECK(r.eof());

		auto err = capture_error([&] { r.get(); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == climap::wire::parse_error_kind::unexpected_eof);
		CHECK(err->offset() == 0);
	}

	SUBCASE("expect")
	{
		std::istringstream in("x");
		climap::wire::reader r(*in.rdbuf());

		CHECK_NOTHROW(r.expect('x'));
		CHECK(r.offset() == 1);
	}

	SUBCASE("expect rejects the wrong byte")
	{
		std::istringstream in("x");
		climap::wire::reader r(*in.rdbuf());

		auto err = capture_error([&] { r.expect('y'); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == climap::wire::parse_error_kind::invalid_character);
		CHECK(err->offset() == 0);
		CHECK(std::string(err->what()).find("expected 'y'") != std::string::npos);
	}

	SUBCASE("valid CRLF")
	{
		std::istringstream in("\r\n");
		climap::wire::reader r(*in.rdbuf());

		CHECK_NOTHROW(r.expect_crlf());
		CHECK(r.offset() == 2);
		CHECK(r.eof());
	}

	SUBCASE("CR without LF")
	{
		std::istringstream in("\rX");
		climap::wire::reader r(*in.rdbuf());

		auto err = capture_error([&] { r.expect_crlf(); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == climap::wire::parse_error_kind::bad_crlf);
		CHECK(err->offset() == 1);
	}

	SUBCASE("LF without CR")
	{
		std::istringstream in("\n");
		climap::wire::reader r(*in.rdbuf());

		auto err = capture_error([&] { r.expect_crlf(); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == climap::wire::parse_error_kind::bad_crlf);
		CHECK(err->offset() == 0);
	}

	SUBCASE("CR at end of input")
	{
		std::istringstream in("\r");
		climap::wire::reader r(*in.rdbuf());

		auto err = capture_error([&] { r.expect_crlf(); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == climap::wire::parse_error_kind::unexpected_eof);
		CHECK(err->offset() == 1);
	}

	SUBCASE("read_exact")
	{
		std::istringstream in("hello");
		climap::wire::reader r(*in.rdbuf());

		char buf[5] = {};
		CHECK_NOTHROW(r.read_exact(buf, sizeof buf));
		CHECK(std::string(buf, sizeof buf) == "hello");
		CHECK(r.offset() == 5);
		CHECK(r.eof());
	}

	SUBCASE("short read_exact")
	{
		std::istringstream in("hi");
		climap::wire::reader r(*in.rdbuf());

		char buf[5] = {};
		auto err = capture_error([&] { r.read_exact(buf, sizeof buf); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == climap::wire::parse_error_kind::unexpected_eof);
		CHECK(err->offset() == 2);
	}

	SUBCASE("read_exact tolerates short underlying reads")
	{
		short_read_streambuf buf("hello");
		climap::wire::reader r(buf);

		char out[5] = {};
		CHECK_NOTHROW(r.read_exact(out, sizeof out));
		CHECK(std::string(out, sizeof out) == "hello");
		CHECK(r.offset() == 5);
	}

	SUBCASE("read_to_crlf")
	{
		std::istringstream in("text\r\nrest");
		climap::wire::reader r(*in.rdbuf());

		CHECK(r.read_to_crlf() == "text");
		CHECK(r.offset() == 6);
		CHECK(r.peek() == 'r');
	}

	SUBCASE("read_to_crlf on an empty line")
	{
		std::istringstream in("\r\n");
		climap::wire::reader r(*in.rdbuf());

		CHECK(r.read_to_crlf().empty());
		CHECK(r.offset() == 2);
	}

	SUBCASE("read_to_crlf without a terminator")
	{
		std::istringstream in("text");
		climap::wire::reader r(*in.rdbuf());

		auto err = capture_error([&] { r.read_to_crlf(); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == climap::wire::parse_error_kind::unexpected_eof);
		CHECK(err->offset() == 4);
	}

	SUBCASE("read_to_crlf enforces max_line_bytes")
	{
		climap::wire::limits lim;
		lim.max_line_bytes = 4;

		std::istringstream in("abcde\r\n");
		climap::wire::reader r(*in.rdbuf(), lim);

		auto err = capture_error([&] { r.read_to_crlf(); });
		REQUIRE(err.has_value());
		CHECK(err->kind() == climap::wire::parse_error_kind::limit_exceeded);
		CHECK(err->offset() == 4);
	}

	SUBCASE("parse errors are std::exceptions")
	{
		std::istringstream in("");
		climap::wire::reader r(*in.rdbuf());

		CHECK_THROWS_AS(r.get(), std::exception);
	}
}
