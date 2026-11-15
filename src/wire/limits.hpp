#pragma once

#include <cstddef>

namespace climap::wire
{

// Caps that keep parsing bounded on hostile or broken input.
struct limits
{
	// Maximum number of bytes in a single literal.
	std::size_t max_literal_bytes = 64 * 1024 * 1024;
	// Maximum nesting depth of parenthesized lists.
	std::size_t max_list_depth = 32;
	// Maximum number of data items collected for one response.
	std::size_t max_items_per_response = 100000;
	// Maximum size of a single atom/astring token.
	std::size_t max_atom_bytes = 64 * 1024;
	// Maximum size of a quoted string.
	std::size_t max_quoted_bytes = 1024 * 1024;
	// Maximum length of a CRLF-terminated line.
	std::size_t max_line_bytes = 64 * 1024;
};

}
