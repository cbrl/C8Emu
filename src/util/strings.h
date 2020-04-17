#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

/*
//----------------------------------------------------------------------------------
// ToStr
//----------------------------------------------------------------------------------
//
// Convert an arithmetic value to a string. For integral values, the base can be
// optionally specified. For floating point values, the format and precision can be
// optionally specified. If the overload using std::chars_format and/or precision
// is used, and the provided type is an integral, it will be converted to a
// floating point value.
//
// Returns std::optional<std::string>, which contains the converted string, unless
// the underlying call to std::to_chars() failed.
// 
//----------------------------------------------------------------------------------

namespace detail {
template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
std::optional<std::string> ToStrImpl(T val, std::chars_format fmt) noexcept {
	// Determine maximum chars possible in string
	constexpr size_t max_chars = std::numeric_limits<T>::digits - std::numeric_limits<T>::min_exponent + 3;

	// Convert the string
	std::array<char, max_chars+1> chars = {'\0'};
	const std::to_chars_result result = std::to_chars(chars.data(), chars.data() + max_chars, val, fmt);

	if (result.ec != std::errc()) {
		return {};
	}

	return std::make_optional<std::string>(chars.data());
}

template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
std::optional<std::string> ToStrImpl(T val, std::chars_format fmt, int precision) noexcept {
	// Determine maximum chars possible in string
	constexpr size_t max_chars = std::numeric_limits<T>::digits - std::numeric_limits<T>::min_exponent + 3;

	// Convert the string
	std::array<char, max_chars+1> chars = {'\0'};
	const std::to_chars_result result = std::to_chars(chars.data(), chars.data() + max_chars, val, fmt, precision);

	if (result.ec != std::errc()) {
		return {};
	}

	return std::make_optional<std::string>(chars.data());
}
} //namespace detail


// Convert the specified integral value to a string with the specified base (default 10)
template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
std::optional<std::string> ToStr(T val, int base = 10) noexcept {

	// Determine maximum chars possible in string
	constexpr size_t max_chars = std::numeric_limits<T>::digits + 1;

	// Convert the string
	std::array<char, max_chars+1> chars = {'\0'};
	const std::to_chars_result result = std::to_chars(chars.data(), chars.data() + max_chars, val, base);

	if (result.ec != std::errc()) {
		return {};
	}

	return std::make_optional<std::string>(chars.data());
}

// Convert the specified floating point value to a string
template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
std::optional<std::string> ToStr(T val) noexcept {

	// Determine maximum chars possible in string
	constexpr size_t max_chars = std::numeric_limits<T>::digits - std::numeric_limits<T>::min_exponent + 3;

	// Convert the string
	std::array<char, max_chars+1> chars = {'\0'};
	const std::to_chars_result result = std::to_chars(chars.data(), chars.data() + max_chars, val);

	if (result.ec != std::errc()) {
		return {};
	}

	return std::make_optional<std::string>(chars.data());
}

// Convert the specified arithemetic value to a string with a specified format
template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
std::optional<std::string> ToStr(T val, std::chars_format fmt) noexcept {

	// Formatted to_chars only accepts floating point values
	if constexpr (std::is_integral_v<T>) {
		if constexpr (sizeof(T) <= sizeof(float))
			return detail::ToStrImpl<float>(val, fmt);
		else if constexpr (sizeof(T) <= sizeof(double))
			return detail::ToStrImpl<double>(val, fmt);
		else
			return detail::ToStrImpl<long double>(val, fmt);
	}
	else {
		return detail::ToStrImpl(val, fmt);
	}
}

// Convert the specified arithemetic value to a string with a specified format and precision
template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
std::optional<std::string> ToStr(T val, std::chars_format fmt, int precision) noexcept {

	// Formatted to_chars only accepts floating point values
	if constexpr (std::is_integral_v<T>) {
		if constexpr (sizeof(T) <= sizeof(float))
			return detail::ToStrImpl<float>(val, fmt, precision);
		else if constexpr (sizeof(T) <= sizeof(double))
			return detail::ToStrImpl<double>(val, fmt, precision);
		else
			return detail::ToStrImpl<long double>(val, fmt, precision);
	}
	else {
		return detail::ToStrImpl(val, fmt, precision);
	}
}



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
template<typename T, typename = std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>>
std::optional<T> StrTo(std::string_view str, int base = 10) noexcept {
	if (str.empty()) {
		return {};
	}

	T out = {};
	const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), out, base);

	if (ptr != (str.data() + str.size())
	    || ec == std::errc::invalid_argument
	    || ec == std::errc::result_out_of_range) {

		return {};
	}

	return out;
}

// Convert a string to the specified floating point type
template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
std::optional<T> StrTo(std::string_view str, std::chars_format fmt = std::chars_format::general) {
	if (str.empty()) {
		return {};
	}

	T out = {};
	const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), out, fmt);

	if (ptr != (str.data() + str.size())
	    || ec == std::errc::invalid_argument
	    || ec == std::errc::result_out_of_range) {

		return {};
	}

	return out;
}

// Convert a string to a boolean value. Accepts numeric values or the case-insensitive strings "true" and "false".
template<typename T, typename = std::enable_if_t<std::is_same_v<T, bool>>>
std::optional<T> StrTo(std::string_view str) noexcept {
	if (str.empty()) {
		return {};
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

    const auto val = StrTo<size_t>(str);
    if (val.has_value()) {
        return static_cast<bool>(val.value());
    }

	return {};
}
*/


//----------------------------------------------------------------------------------
// Trim
//----------------------------------------------------------------------------------
//
// Trim the specified character from the left, right, or both ends of a string.
// A specialized overload for trimming whitespace characters is given. Versions
// are given for in-place trimming, or trimming a copy of the input string.
//
//----------------------------------------------------------------------------------

namespace detail {
template<typename StringT, typename CompareFuncT>
void LTrimImpl(StringT& str, CompareFuncT&& compare_func) {
	str.erase(str.begin(), std::find_if(str.begin(), str.end(), compare_func));
}

template<typename StringT, typename CompareFuncT>
void RTrimImpl(StringT& str, CompareFuncT&& compare_func) {
	str.erase(std::find_if(str.rbegin(), str.rend(), compare_func).base(), str.end());
}

template<typename StringT, typename CompareFuncT>
void TrimImpl(StringT& str, CompareFuncT&& compare_func) {
	LTrimImpl(str, compare_func);
	RTrimImpl(str, compare_func);
}
} //namespace detail


// Trim the specified token from the left of a string
template<typename StringT>
void LTrim(StringT& str, typename StringT::value_type token) {
	detail::LTrimImpl(
		str,
		[&token](typename StringT::value_type ch) {
			return ch != token;
		}
	);
}

// Trim the specified token from the right of a string
template<typename StringT>
void RTrim(StringT& str, typename StringT::value_type token) {
	detail::RTrimImpl(
		str,
		[&token](typename StringT::value_type ch) {
			return ch != token;
		}
	);
}

// Trim the specified token at the beginning and end of a string
template<typename StringT>
void Trim(StringT& str, typename StringT::value_type token) {
	LTrim(str, token);
	RTrim(str, token);
}

// Trim whitespace at the beginning and end of a string
template<typename StringT>
void TrimWhiteSpace(StringT& str) {
	detail::TrimImpl(
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
std::vector<std::basic_string<CharT>> Split(std::basic_string_view<CharT> str, std::basic_string_view<CharT> token) {
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


inline std::vector<std::string> Split(std::string_view str, std::string_view token) {
    return detail::Split(str, token);
}
inline std::vector<std::wstring> Split(std::wstring_view str, std::wstring_view token) {
    return detail::Split(str, token);
}
#if __cplusplus > 201703L
inline std::vector<std::u8string> Split(std::u8string_view str, std::u8string_view token) {
    return detail::Split(str, token);
}
#endif
inline std::vector<std::u16string> Split(std::u16string_view str, std::u16string_view token) {
    return detail::Split(str, token);
}
inline std::vector<std::u32string> Split(std::u32string_view str, std::u32string_view token) {
    return detail::Split(str, token);
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
void Replace(StringT& str, std::basic_string_view<typename StringT::value_type> from, std::basic_string_view<typename StringT::value_type> to) {
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


inline void Replace(std::string& str, std::string_view from, std::string_view to) {
    detail::Replace<std::string>(str, from, to);
}
inline void Replace(std::wstring& str, std::wstring_view from, std::wstring_view to) {
    detail::Replace<std::wstring>(str, from, to);
}
#if __cplusplus > 201703L
inline void Replace(std::u8string& str, std::u8string_view from, std::u8string_view to) {
    detail::Replace<std::u8string>(str, from, to);
}
#endif
inline void Replace(std::u16string& str, std::u16string_view from, std::u16string_view to) {
    detail::Replace<std::u16string>(str, from, to);
}
inline void Replace(std::u32string& str, std::u32string_view from, std::u32string_view to) {
    detail::Replace<std::u32string>(str, from, to);
}
