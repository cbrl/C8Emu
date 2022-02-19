#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>


//----------------------------------------------------------------------------------
// Trim
//----------------------------------------------------------------------------------
//
// trim the specified character from the left, right, or both ends of a string.
// A specialized overload for trimming whitespace characters is given. Versions
// are given for in-place trimming, or trimming a copy of the input string.
//
//----------------------------------------------------------------------------------

namespace detail {
template<typename StringT, typename CompareFuncT>
auto ltrim_impl(StringT& str, CompareFuncT&& compare_func) -> void {
	str.erase(str.begin(), std::find_if(str.begin(), str.end(), compare_func));
}

template<typename StringT, typename CompareFuncT>
auto rtrim_impl(StringT& str, CompareFuncT&& compare_func) -> void {
	str.erase(std::find_if(str.rbegin(), str.rend(), compare_func).base(), str.end());
}

template<typename StringT, typename CompareFuncT>
auto trim_impl(StringT& str, CompareFuncT&& compare_func) -> void {
	ltrim_impl(str, compare_func);
	rtrim_impl(str, compare_func);
}
} //namespace detail


// Trim the specified token from the left of a string
template<typename StringT>
auto ltrim(StringT& str, typename StringT::value_type token) -> void {
	detail::ltrim_impl(
		str,
		[&token](typename StringT::value_type ch) {
			return ch != token;
		}
	);
}

// trim the specified token from the right of a string
template<typename StringT>
auto rtrim(StringT& str, typename StringT::value_type token) -> void {
	detail::rtrim_impl(
		str,
		[&token](typename StringT::value_type ch) {
			return ch != token;
		}
	);
}

// Trim the specified token at the beginning and end of a string
template<typename StringT>
auto trim(StringT& str, typename StringT::value_type token) -> void {
	ltrim(str, token);
	rtrim(str, token);
}

// Trim whitespace at the beginning and end of a string
template<typename StringT>
auto trim_whitespace(StringT& str) -> void {
	detail::trim_impl(
		str,
		[](typename StringT::value_type ch) {
			return !std::isspace(ch);
		}
	);
}



//----------------------------------------------------------------------------------
// Split
//----------------------------------------------------------------------------------
//
// Split a string at each occurrence of the specified token. Returns a vector
// of the resultant strings.
//
//----------------------------------------------------------------------------------

namespace detail {
template<typename CharT>
auto split(std::basic_string_view<CharT> str, std::basic_string_view<CharT> token) -> std::vector<std::basic_string<CharT>> {
	std::vector<std::basic_string<CharT>> out;

	size_t start = 0;
	size_t end = str.find(token);

	while (end != std::basic_string_view<CharT>::npos) {
		out.emplace_back(str.substr(start, end - start));
		start = end + token.length();
		end = str.find(token, start);
	}
	out.emplace_back(str.substr(start, end));

	return out;
}
} //namespace detail


inline auto split(std::string_view str, std::string_view token) -> std::vector<std::string> {
    return detail::split(str, token);
}
inline auto split(std::wstring_view str, std::wstring_view token) -> std::vector<std::wstring> {
    return detail::split(str, token);
}
inline auto split(std::u8string_view str, std::u8string_view token) -> std::vector<std::u8string> {
    return detail::split(str, token);
}
inline auto split(std::u16string_view str, std::u16string_view token) -> std::vector<std::u16string> {
    return detail::split(str, token);
}
inline auto split(std::u32string_view str, std::u32string_view token) -> std::vector<std::u32string> {
    return detail::split(str, token);
}



//----------------------------------------------------------------------------------
// Replace
//----------------------------------------------------------------------------------
//
// Replace each occurance of a token in a string with another token. Versions are
// given for in-place replacement, or replacement on a copy of the input string.
//
//----------------------------------------------------------------------------------

namespace detail {
template<typename StringT>
auto replace(StringT& str, std::basic_string_view<typename StringT::value_type> from, std::basic_string_view<typename StringT::value_type> to) -> void {
    if (str.empty()) {
        return;
    }

    size_t start = 0;
    while ((start = str.find(from, start)) != StringT::npos) {
        str.replace(start, from.length(), to);
        start += to.length();
    }
}
} //namespace detail


inline auto replace(std::string& str, std::string_view from, std::string_view to) -> void {
    detail::replace<std::string>(str, from, to);
}
inline auto replace(std::wstring& str, std::wstring_view from, std::wstring_view to) -> void {
    detail::replace<std::wstring>(str, from, to);
}
inline auto replace(std::u8string& str, std::u8string_view from, std::u8string_view to) -> void {
    detail::replace<std::u8string>(str, from, to);
}
inline auto replace(std::u16string& str, std::u16string_view from, std::u16string_view to) -> void {
    detail::replace<std::u16string>(str, from, to);
}
inline auto replace(std::u32string& str, std::u32string_view from, std::u32string_view to) -> void {
    detail::replace<std::u32string>(str, from, to);
}
