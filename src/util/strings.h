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
// StrTo
//----------------------------------------------------------------------------------
//
// Convert a string to a specified arithmetic type. For boolean conversion,
// valid strings are the case-insensitive "true" and "false", or any numeric value.
//
// Returns an std::optional<T>, which contains the converted value unless the
// string was empty or the underlying call to std::from_chars() failed.
//
//----------------------------------------------------------------------------------

// Convert a string to the specified integral type
template<std::integral T> requires !std::same_as<T, bool>
auto str_to(std::string_view str, int base = 10) noexcept -> std::optional<T> {
	if (str.empty()) {
		return std::nullopt;
	}

	auto out = T{};
	const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), out, base);

	if (ptr != (str.data() + str.size())
	    || ec == std::errc::invalid_argument
	    || ec == std::errc::result_out_of_range) {

		return std::nullopt;
	}

	return out;
}

// Convert a string to the specified floating point type
template<std::floating_point T>
auto str_to(std::string_view str, std::chars_format fmt = std::chars_format::general) -> std::optional<T> {
	if (str.empty()) {
		return std::nullopt;
	}

	auto out = T{};
	const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), out, fmt);

	if (ptr != (str.data() + str.size())
	    || ec == std::errc::invalid_argument
	    || ec == std::errc::result_out_of_range) {

		return std::nullopt;
	}

	return out;
}

// Convert a string to a boolean value. Accepts numeric values or the case-insensitive strings "true" and "false".
template<typename T> requires std::same_as<T, bool>
auto str_to(std::string_view str) noexcept -> std::optional<T> {
	if (str.empty()) {
		return std::nullopt;
	}

	if (str.size() == 4
	    && (str[0] == 't' || str[0] == 'T')
	    && (str[1] == 'r' || str[1] == 'R')
	    && (str[2] == 'u' || str[2] == 'U')
	    && (str[3] == 'e' || str[3] == 'E')) {

		return true;
	}
	if (str.size() == 5
	    && (str[0] == 'f' || str[0] == 'F')
	    && (str[1] == 'a' || str[1] == 'A')
	    && (str[2] == 'l' || str[2] == 'L')
	    && (str[3] == 's' || str[3] == 'S')
	    && (str[4] == 'e' || str[4] == 'E')) {

		return false;
	}

	const auto val = str_to<size_t>(str);
	if (val.has_value()) {
		return static_cast<bool>(val.value());
	}

	return std::nullopt;
}


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
