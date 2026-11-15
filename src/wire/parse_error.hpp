#pragma once

#include <cstddef>
#include <exception>
#include <string>
#include <utility>

namespace climap::wire
{

enum class parse_error_kind
{
	unexpected_eof,
	bad_crlf,
	invalid_character,
	limit_exceeded,
	number_overflow,
};

class parse_error : public std::exception
{
public:
	parse_error(parse_error_kind kind, std::size_t offset, std::string message)
		: kind_(kind), offset_(offset), message_(std::move(message))
	{
	}

	const char *what() const noexcept override { return message_.c_str(); }

	parse_error_kind kind() const noexcept { return kind_; }

	// Byte offset of the offending input, counted from the start of the reader.
	std::size_t offset() const noexcept { return offset_; }

private:
	parse_error_kind kind_;
	std::size_t offset_;
	std::string message_;
};

}
