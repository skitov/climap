#pragma once
#include <cctype>
#include <algorithm>
#include <string>

namespace lib
{

inline bool chr_eq_ignore_case(char a, char b)
{
	return std::tolower(static_cast<unsigned char>(a)) ==
		std::tolower(static_cast<unsigned char>(b));
}

inline bool str_eq_ignore_case(const std::string& a, const std::string& b)
{
	return std::equal(a.begin(), a.end(), b.begin(), b.end(), chr_eq_ignore_case);
}

}
