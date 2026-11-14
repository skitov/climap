// Compile gate for the command-independent wire layer.
//
// This translation unit is built as an object library by the `check_headers`
// target so that headers cannot silently rot when no test includes them.
// Headers still under construction (response_schema, response_collection,
// response_validator) are intentionally not listed until they are either
// repaired or retired.

#include "lib/util.hpp"
#include "wire/data_item.hpp"
#include "wire/parser_exception.hpp"
#include "wire/response_stream.hpp"
