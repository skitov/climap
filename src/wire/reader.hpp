#pragma once

#include <cstddef>
#include <streambuf>
#include <string>

#include "limits.hpp"
#include "parse_error.hpp"

namespace climap::wire
{

// Buffered byte source for the wire layer.
//
// The reader owns no parse state: no delimiter bookkeeping, no current
// response flag. Callers decide which delimiters are legal in their context,
// which keeps nested lists and literals unambiguous.
class reader
{
public:
	explicit reader(std::streambuf &buffer, limits lim = {})
		: buffer_(buffer), lim_(lim)
	{
	}

	reader(const reader &) = delete;
	reader &operator=(const reader &) = delete;

	const limits &limit_config() const noexcept { return lim_; }

	std::size_t offset() const noexcept { return offset_; }

	// Returns the next byte without consuming it, or EOF sentinel at the end.
	int peek() { return buffer_.sgetc(); }

	bool eof() { return peek() == eof_value; }

	// Consumes one byte. Throws unexpected_eof if the input is exhausted.
	int get()
	{
		const int c = buffer_.sbumpc();
		if (c == eof_value)
			throw parse_error(parse_error_kind::unexpected_eof, offset_,
			                  "unexpected end of input");
		++offset_;
		return c;
	}

	// Consumes one byte and requires it to equal `expected`.
	void expect(int expected)
	{
		const std::size_t at = offset_;
		const int c = get();
		if (c != expected)
			throw parse_error(parse_error_kind::invalid_character, at,
			                  "expected '" + char_to_string(expected) + "', got '" +
			                      char_to_string(c) + "'");
	}

	// Consumes CRLF, rejecting every other sequence.
	void expect_crlf()
	{
		const std::size_t at = offset_;
		if (get() != '\r')
			throw parse_error(parse_error_kind::bad_crlf, at, "expected CR");

		const std::size_t lf_at = offset_;
		if (get() != '\n')
			throw parse_error(parse_error_kind::bad_crlf, lf_at,
			                  "expected LF after CR");
	}

	// Reads exactly `size` bytes; short input is an unexpected_eof.
	void read_exact(char *dst, std::size_t size)
	{
		std::size_t total = 0;
		while (total < size)
		{
			const std::streamsize got = buffer_.sgetn(
				dst + total, static_cast<std::streamsize>(size - total));
			if (got <= 0)
				break;
			total += static_cast<std::size_t>(got);
		}
		offset_ += total;
		if (total != size)
			throw parse_error(parse_error_kind::unexpected_eof, offset_,
			                  "unexpected end of input while reading " +
			                      std::to_string(size) + " bytes");
	}

	// Reads up to (but not including) the next CRLF, which is consumed.
	std::string read_to_crlf()
	{
		std::string line;
		while (true)
		{
			const int c = peek();
			if (c == eof_value)
				throw parse_error(parse_error_kind::unexpected_eof, offset_,
				                  "unexpected end of input before CRLF");
			if (c == '\r')
			{
				expect_crlf();
				return line;
			}
			if (line.size() >= lim_.max_line_bytes)
				throw parse_error(parse_error_kind::limit_exceeded, offset_,
				                  "line exceeds max_line_bytes (" +
				                      std::to_string(lim_.max_line_bytes) + ")");
			line.push_back(static_cast<char>(get()));
		}
	}

private:
	static constexpr int eof_value = std::streambuf::traits_type::eof();

	static std::string char_to_string(int c)
	{
		if (c == eof_value)
			return "EOF";
		return std::string(1, static_cast<char>(c));
	}

	std::streambuf &buffer_;
	const limits lim_;
	std::size_t offset_ = 0;
};

}
