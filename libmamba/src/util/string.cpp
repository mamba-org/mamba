// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <cctype>
#include <cstddef>
#include <cwchar>
#include <cwctype>
#include <iterator>
#include <stdexcept>

#include "mamba/util/string.hpp"

namespace mamba::util
{
    /****************************************
     *  Implementation of cctype functions  *
     ****************************************/

    auto is_control(char c) -> bool
    {
        return std::iscntrl(static_cast<unsigned char>(c)) != 0;
    }

    auto is_control(wchar_t c) -> bool
    {
        return std::iswcntrl(static_cast<wint_t>(c)) != 0;
    }

    auto is_print(char c) -> bool
    {
        return std::isprint(static_cast<unsigned char>(c)) != 0;
    }

    auto is_print(wchar_t c) -> bool
    {
        return std::iswprint(static_cast<wint_t>(c)) != 0;
    }

    auto is_space(char c) -> bool
    {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    }

    auto is_space(wchar_t c) -> bool
    {
        return std::iswspace(static_cast<wint_t>(c)) != 0;
    }

    auto is_blank(char c) -> bool
    {
        return std::isblank(static_cast<unsigned char>(c)) != 0;
    }

    auto is_blank(wchar_t c) -> bool
    {
        return std::iswblank(static_cast<wint_t>(c)) != 0;
    }

    auto is_graphic(char c) -> bool
    {
        return std::isgraph(static_cast<unsigned char>(c)) != 0;
    }

    auto is_graphic(wchar_t c) -> bool
    {
        return std::iswgraph(static_cast<wint_t>(c)) != 0;
    }

    auto is_digit(char c) -> bool
    {
        return std::isdigit(static_cast<unsigned char>(c)) != 0;
    }

    auto is_digit(wchar_t c) -> bool
    {
        return std::iswdigit(static_cast<wint_t>(c)) != 0;
    }

    auto is_punct(char c) -> bool
    {
        return std::ispunct(static_cast<unsigned char>(c)) != 0;
    }

    auto is_punct(wchar_t c) -> bool
    {
        return std::iswpunct(static_cast<wint_t>(c)) != 0;
    }

    auto is_alpha(char c) -> bool
    {
        return std::isalpha(static_cast<unsigned char>(c)) != 0;
    }

    auto is_alpha(wchar_t c) -> bool
    {
        return std::iswalpha(static_cast<wint_t>(c)) != 0;
    }

    auto is_alphanum(char c) -> bool
    {
        return std::isalnum(static_cast<unsigned char>(c)) != 0;
    }

    auto is_alphanum(wchar_t c) -> bool
    {
        return std::iswalnum(static_cast<wint_t>(c)) != 0;
    }

    auto is_lower(char c) -> bool
    {
        return std::islower(static_cast<unsigned char>(c)) != 0;
    }

    auto is_lower(wchar_t c) -> bool
    {
        return std::iswlower(static_cast<wint_t>(c)) != 0;
    }

    auto is_upper(char c) -> bool
    {
        return std::isupper(static_cast<unsigned char>(c)) != 0;
    }

    auto is_upper(wchar_t c) -> bool
    {
        return std::iswupper(static_cast<wint_t>(c)) != 0;
    }

    auto to_lower(char c) -> char
    {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    auto to_lower(wchar_t c) -> wchar_t
    {
        return static_cast<wchar_t>(std::towlower(static_cast<wint_t>(c)));
    }

    auto to_upper(char c) -> char
    {
        return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    auto to_upper(wchar_t c) -> wchar_t
    {
        return static_cast<wchar_t>(std::towupper(static_cast<wint_t>(c)));
    }

    /***************************************************
     *  Implementation of to_lower to_upper functions  *
     ***************************************************/

    namespace
    {
        template <typename Char>
        auto to_lower_impl(std::basic_string_view<Char> str) -> std::basic_string<Char>
        {
            auto out = std::basic_string<Char>();
            std::transform(
                str.cbegin(),
                str.cend(),
                std::back_inserter(out),
                [](auto c) { return to_lower(c); }
            );
            return out;
        }
    }

    auto to_lower(std::string_view str) -> std::string
    {
        return to_lower_impl(str);
    }

    auto to_lower(std::wstring_view str) -> std::wstring
    {
        return to_lower_impl(str);
    }

    template <typename Char>
    auto to_lower(std::basic_string<Char>&& str) -> std::basic_string<Char>
    {
        std::transform(str.cbegin(), str.cend(), str.begin(), [](auto c) { return to_lower(c); });
        return str;
    }

    template std::string to_lower(std::string&& str);
    template std::wstring to_lower(std::wstring&& str);

    namespace
    {
        template <typename Char>
        auto to_upper_impl(std::basic_string_view<Char> str) -> std::basic_string<Char>
        {
            auto out = std::basic_string<Char>();
            std::transform(
                str.cbegin(),
                str.cend(),
                std::back_inserter(out),
                [](auto c) { return to_upper(c); }
            );
            return out;
        }
    }

    auto to_upper(std::string_view str) -> std::string
    {
        return to_upper_impl(str);
    }

    auto to_upper(std::wstring_view str) -> std::wstring
    {
        return to_upper_impl(str);
    }

    template <typename Char>
    auto to_upper(std::basic_string<Char>&& str) -> std::basic_string<Char>
    {
        std::transform(str.cbegin(), str.cend(), str.begin(), [](auto c) { return to_upper(c); });
        return str;
    }

    template std::string to_upper(std::string&& str);
    template std::wstring to_upper(std::wstring&& str);

    /*******************************************
     *  Implementation of start_with functions  *
     *******************************************/

    // TODO(C++20) This is a method of string_view
    auto contains(std::string_view str, std::string_view sub_str) -> bool
    {
        return str.find(sub_str) != std::string::npos;
    }

    // TODO(C++20) This is a method of string_view
    auto contains(std::string_view str, char c) -> bool
    {
        return str.find(c) != std::string::npos;
    }

    auto contains(char c1, char c2) -> bool
    {
        return c1 == c2;
    }

    // TODO(C++20) This is a method of string_view
    auto ends_with(std::string_view str, std::string_view suffix) -> bool
    {
        return str.size() >= suffix.size()
               && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
    }

    auto ends_with(std::string_view str, std::string_view::value_type c) -> bool
    {
        return (!str.empty()) && (str.back() == c);
    }

    // TODO(C++20) This is a method of string_view
    auto starts_with(std::string_view str, std::string_view prefix) -> bool
    {
        return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
    }

    // TODO(C++20) This is a method of string_view
    auto starts_with(std::string_view str, std::string_view::value_type c) -> bool
    {
        return (!str.empty()) && (str.front() == c);
    }

    /*************************************************
     *  Implementation of starts_with any functions  *
     *************************************************/

    template bool any_starts_with(const std::vector<std::string>&, std::string_view);
    template bool any_starts_with(const std::vector<std::string_view>&, std::string_view);

    template bool starts_with_any(std::string_view, const std::vector<std::string>&);
    template bool starts_with_any(std::string_view, const std::vector<std::string_view>&);

    /******************************************************
     *  Implementation of remove prefix/suffix functions  *
     ******************************************************/

    auto split_prefix(std::string_view str, std::string_view prefix)
        -> std::array<std::string_view, 2>
    {
        if (starts_with(str, prefix))
        {
            return { str.substr(0, prefix.size()), str.substr(prefix.size()) };
        }
        return { std::string_view(), str };
    }

    auto split_prefix(std::string_view str, std::string_view::value_type c)
        -> std::array<std::string_view, 2>
    {
        if (starts_with(str, c))
        {
            return { str.substr(0, 1), str.substr(1) };
        }
        return { std::string_view(), str };
    }

    auto remove_prefix(std::string_view str, std::string_view prefix) -> std::string_view
    {
        return std::get<1>(split_prefix(str, prefix));
    }

    auto remove_prefix(std::string_view str, std::string_view::value_type c) -> std::string_view
    {
        return std::get<1>(split_prefix(str, c));
    }

    auto split_suffix(std::string_view str, std::string_view suffix)
        -> std::array<std::string_view, 2>
    {
        if (ends_with(str, suffix))
        {
            auto suffix_pos = str.size() - suffix.size();
            return { str.substr(0, suffix_pos), str.substr(suffix_pos) };
        }
        return { str, std::string_view() };
    }

    auto split_suffix(std::string_view str, std::string_view::value_type c)
        -> std::array<std::string_view, 2>
    {
        if (ends_with(str, c))
        {
            auto suffix_pos = str.size() - 1;
            return { str.substr(0, suffix_pos), str.substr(suffix_pos) };
        }
        return { str, std::string_view() };
    }

    auto remove_suffix(std::string_view str, std::string_view suffix) -> std::string_view
    {
        return std::get<0>(split_suffix(str, suffix));
    }

    auto remove_suffix(std::string_view str, std::string_view::value_type c) -> std::string_view
    {
        return std::get<0>(split_suffix(str, c));
    }

    /***************************************
     *  Implementation of strip functions  *
     ***************************************/

    auto lstrip(std::string_view input, char c) -> std::string_view
    {
        return lstrip_parts(input, c)[1];
    }

    auto lstrip(std::wstring_view input, wchar_t c) -> std::wstring_view
    {
        return lstrip_parts(input, c)[1];
    }

    auto lstrip(std::string_view input, std::string_view chars) -> std::string_view
    {
        return lstrip_parts(input, chars)[1];
    }

    auto lstrip(std::wstring_view input, std::wstring_view chars) -> std::wstring_view
    {
        return lstrip_parts(input, chars)[1];
    }

    auto lstrip(std::string_view input) -> std::string_view
    {
        using Char = decltype(input)::value_type;
        return lstrip_if(input, [](Char c) { return !is_graphic(c); });
    }

    auto lstrip(std::wstring_view input) -> std::wstring_view
    {
        using Char = decltype(input)::value_type;
        return lstrip_if(input, [](Char c) { return !is_graphic(c); });
    }

    auto rstrip(std::string_view input, char c) -> std::string_view
    {
        return rstrip_parts(input, c)[0];
    }

    auto rstrip(std::wstring_view input, wchar_t c) -> std::wstring_view
    {
        return rstrip_parts(input, c)[0];
    }

    auto rstrip(std::string_view input, std::string_view chars) -> std::string_view
    {
        return rstrip_parts(input, chars)[0];
    }

    auto rstrip(std::wstring_view input, std::wstring_view chars) -> std::wstring_view
    {
        return rstrip_parts(input, chars)[0];
    }

    auto rstrip(std::string_view input) -> std::string_view
    {
        using Char = decltype(input)::value_type;
        return rstrip_if(input, [](Char c) { return !is_graphic(c); });
    }

    auto rstrip(std::wstring_view input) -> std::wstring_view
    {
        using Char = decltype(input)::value_type;
        return rstrip_if(input, [](Char c) { return !is_graphic(c); });
    }

    auto strip(std::string_view input, char c) -> std::string_view
    {
        return strip_parts(input, c)[1];
    }

    auto strip(std::wstring_view input, wchar_t c) -> std::wstring_view
    {
        return strip_parts(input, c)[1];
    }

    auto strip(std::string_view input, std::string_view chars) -> std::string_view
    {
        return strip_parts(input, chars)[1];
    }

    auto strip(std::wstring_view input, std::wstring_view chars) -> std::wstring_view
    {
        return strip_parts(input, chars)[1];
    }

    auto strip(std::string_view input) -> std::string_view
    {
        using Char = decltype(input)::value_type;
        return strip_if(input, [](Char c) { return !is_graphic(c); });
    }

    auto strip(std::wstring_view input) -> std::wstring_view
    {
        using Char = decltype(input)::value_type;
        return strip_if(input, [](Char c) { return !is_graphic(c); });
    }

    void inplace_strip(std::string& input)
    {
        if (input.empty())
        {
            return;
        }

        const auto start = input.find_first_not_of(" \t\n\v\f\r");

        if (start == std::string::npos)
        {
            input.clear();
            return;
        }

        const auto end = input.find_last_not_of(" \t\n\v\f\r");

        input = input.substr(start, end - start + 1);
    }

    /*********************************************
     *  Implementation of strip_parts functions  *
     *********************************************/

    namespace
    {
        // string_view has a different overload for ``find(char)`` and ``find(string_view)``
        // so we want to leverage that.
        template <typename Char, typename CharOrStrView>
        auto lstrip_parts_impl(std::basic_string_view<Char> input, CharOrStrView chars)
            -> std::array<std::basic_string_view<Char>, 2>
        {
            const std::size_t start = input.find_first_not_of(chars);
            if (start == std::basic_string_view<Char>::npos)
            {
                return { input, std::basic_string_view<Char>{} };
            }
            return { input.substr(0, start), input.substr(start) };
        }
    }

    auto lstrip_parts(std::string_view input, char c) -> std::array<std::string_view, 2>
    {
        return lstrip_parts_impl(input, c);
    }

    auto lstrip_parts(std::wstring_view input, wchar_t c) -> std::array<std::wstring_view, 2>
    {
        return lstrip_parts_impl(input, c);
    }

    auto lstrip_parts(std::string_view input, std::string_view chars)
        -> std::array<std::string_view, 2>
    {
        return lstrip_parts_impl(input, chars);
    }

    auto lstrip_parts(std::wstring_view input, std::wstring_view chars)
        -> std::array<std::wstring_view, 2>
    {
        return lstrip_parts_impl(input, chars);
    }

    namespace
    {
        // string_view has a different overload for ``find(char)`` and ``find(string_view)``
        // so we want to leverage that.
        template <typename Char, typename CharOrStrView>
        auto rstrip_parts_impl(std::basic_string_view<Char> input, CharOrStrView chars)
            -> std::array<std::basic_string_view<Char>, 2>
        {
            const std::size_t end = input.find_last_not_of(chars);
            if (end == std::basic_string_view<Char>::npos)
            {
                return { std::basic_string_view<Char>{}, input };
            }
            return { input.substr(0, end + 1), input.substr(end + 1) };
        }
    }

    auto rstrip_parts(std::string_view input, char c) -> std::array<std::string_view, 2>
    {
        return rstrip_parts_impl(input, c);
    }

    auto rstrip_parts(std::wstring_view input, wchar_t c) -> std::array<std::wstring_view, 2>
    {
        return rstrip_parts_impl(input, c);
    }

    auto rstrip_parts(std::string_view input, std::string_view chars)
        -> std::array<std::string_view, 2>
    {
        return rstrip_parts_impl(input, chars);
    }

    auto rstrip_parts(std::wstring_view input, std::wstring_view chars)
        -> std::array<std::wstring_view, 2>
    {
        return rstrip_parts_impl(input, chars);
    }

    namespace
    {
        // string_view has a different overload for ``find(char)`` and ``find(string_view)``
        // so we want to leverage that.
        template <typename Char, typename CharOrStrView>
        auto strip_parts_impl(std::basic_string_view<Char> input, CharOrStrView chars)
            -> std::array<std::basic_string_view<Char>, 3>
        {
            const std::size_t start = input.find_first_not_of(chars);
            if (start == std::basic_string_view<Char>::npos)
            {
                return { input, {}, {} };
            }
            const std::size_t end = input.find_last_not_of(chars) + 1;
            const std::size_t length = end - start;
            return { input.substr(0, start), input.substr(start, length), input.substr(end) };
        }
    }

    auto strip_parts(std::string_view input, char c) -> std::array<std::string_view, 3>
    {
        return strip_parts_impl(input, c);
    }

    auto strip_parts(std::wstring_view input, wchar_t c) -> std::array<std::wstring_view, 3>
    {
        return strip_parts_impl(input, c);
    }

    auto strip_parts(std::string_view input, std::string_view chars)
        -> std::array<std::string_view, 3>
    {
        return strip_parts_impl(input, chars);
    }

    auto strip_parts(std::wstring_view input, std::wstring_view chars)
        -> std::array<std::wstring_view, 3>
    {
        return strip_parts_impl(input, chars);
    }

    /********************************************
     *  Implementation of split_once functions  *
     ********************************************/

    namespace
    {
        template <typename Char, typename CharOrStrView>
        auto split_once_impl(std::basic_string_view<Char> str, CharOrStrView sep)
            -> std::tuple<std::string_view, std::optional<std::string_view>>
        {
            static constexpr auto npos = std::basic_string_view<Char>::npos;
            if (const auto pos = str.find(sep); pos != npos)
            {
                return { str.substr(0, pos), str.substr(pos + detail::length(sep)) };
            }
            return { str, std::nullopt };
        }
    }

    auto split_once(std::string_view str, char sep)
        -> std::tuple<std::string_view, std::optional<std::string_view>>
    {
        return split_once_impl(str, sep);
    }

    auto split_once(std::string_view str, std::string_view sep)
        -> std::tuple<std::string_view, std::optional<std::string_view>>
    {
        return split_once_impl(str, sep);
    }

    namespace
    {
        template <typename Char, typename CharOrStrView>
        auto rsplit_once_impl(std::basic_string_view<Char> str, CharOrStrView sep)
            -> std::tuple<std::optional<std::string_view>, std::string_view>
        {
            static constexpr auto npos = std::basic_string_view<Char>::npos;
            if (const auto pos = str.rfind(sep); pos != npos)
            {
                return { str.substr(0, pos), str.substr(pos + detail::length(sep)) };
            }
            return { std::nullopt, str };
        }
    }

    auto rsplit_once(std::string_view str, char sep)
        -> std::tuple<std::optional<std::string_view>, std::string_view>
    {
        return rsplit_once_impl(str, sep);
    }

    auto rsplit_once(std::string_view str, std::string_view sep)
        -> std::tuple<std::optional<std::string_view>, std::string_view>
    {
        return rsplit_once_impl(str, sep);
    }

    /***************************************************
     *  Implementation of split_once_on_any functions  *
     ***************************************************/

    auto split_once_on_any(std::string_view str, std::string_view many_seps)
        -> std::tuple<std::string_view, std::optional<std::string_view>>
    {
        static constexpr auto npos = std::string_view::npos;
        if (const auto pos = str.find_first_of(many_seps); pos != npos)
        {
            return { str.substr(0, pos), str.substr(pos + 1) };
        }
        return { str, std::nullopt };
    }

    auto rsplit_once_on_any(std::string_view str, std::string_view many_seps)
        -> std::tuple<std::optional<std::string_view>, std::string_view>
    {
        static constexpr auto npos = std::string_view::npos;
        if (const auto pos = str.find_last_of(many_seps); pos != npos)
        {
            return { str.substr(0, pos), str.substr(pos + 1) };
        }
        return { std::nullopt, str };
    }

    /***************************************
     *  Implementation of split functions  *
     ***************************************/

    namespace
    {
        template <class Char>
        auto split(
            const std::basic_string_view<Char> input,
            const std::basic_string_view<Char> sep,
            std::size_t max_split = SIZE_MAX
        ) -> std::vector<std::basic_string<Char>>
        {
            if (sep.size() < 1)
            {
                throw std::invalid_argument("Separator must have size greater than 0");
            }

            std::vector<std::basic_string<Char>> result;

            const std::size_t len = input.size();
            const std::size_t n = sep.size();
            std::size_t i = 0;
            std::size_t j = 0;

            while (i + n <= len)
            {
                if (input[i] == sep[0] && input.substr(i, n) == sep)
                {
                    if (max_split-- <= 0)
                    {
                        break;
                    }
                    result.emplace_back(input.substr(j, i - j));
                    i = j = i + n;
                }
                else
                {
                    i++;
                }
            }
            result.emplace_back(input.substr(j, len - j));
            return result;
        }

        template <class Char>
        auto rsplit(
            const std::basic_string_view<Char> input,
            const std::basic_string_view<Char> sep,
            std::size_t max_split = SIZE_MAX
        ) -> std::vector<std::basic_string<Char>>
        {
            if (max_split == SIZE_MAX)
            {
                return split(input, sep, max_split);
            }
            if (sep.size() < 1)
            {
                throw std::invalid_argument("Separator must have size greater than 0");
            }

            std::vector<std::basic_string<Char>> result;

            const std::size_t len = input.size();
            const std::size_t n = sep.size();
            std::size_t i = len;
            std::size_t j = len;

            while (i >= n)
            {
                if (input[i - 1] == sep[n - 1] && input.substr(i - n, n) == sep)
                {
                    if (max_split-- <= 0)
                    {
                        break;
                    }
                    result.emplace_back(input.substr(i, j - i));
                    i = j = i - n;
                }
                else
                {
                    i--;
                }
            }
            result.emplace_back(input.substr(0, j));
            std::reverse(result.begin(), result.end());

            return result;
        }
    }

    // TODO(C++20) lazy_split_view is a range
    auto split(std::string_view input, std::string_view sep, std::size_t max_split)
        -> std::vector<std::string>
    {
        return split<decltype(input)::value_type>(input, sep, max_split);
    }

    // TODO(C++20) lazy_split_view is a range
    auto split(std::string_view input, char sep, std::size_t max_split) -> std::vector<std::string>
    {
        const auto sep_arr = std::array<char, 2>{ sep, '\0' };
        return split<decltype(input)::value_type>(input, sep_arr.data(), max_split);
    }

    // TODO(C++20) lazy_split_view is a range
    auto split(std::wstring_view input, std::wstring_view sep, std::size_t max_split)
        -> std::vector<std::wstring>
    {
        return split<decltype(input)::value_type>(input, sep, max_split);
    }

    // TODO(C++20) lazy_split_view is a range
    auto split(std::wstring_view input, wchar_t sep, std::size_t max_split)
        -> std::vector<std::wstring>
    {
        const auto sep_arr = std::array<wchar_t, 2>{ sep, L'\0' };
        return split<decltype(input)::value_type>(input, sep_arr.data(), max_split);
    }

    // TODO(C++20) lazy_split_view is a range
    auto rsplit(std::string_view input, std::string_view sep, std::size_t max_split)
        -> std::vector<std::string>
    {
        return rsplit<decltype(input)::value_type>(input, sep, max_split);
    }

    // TODO(C++20) lazy_split_view is a range
    auto rsplit(std::string_view input, char sep, std::size_t max_split) -> std::vector<std::string>
    {
        const auto sep_arr = std::array<char, 2>{ sep, '\0' };
        return rsplit<decltype(input)::value_type>(input, sep_arr.data(), max_split);
    }

    // TODO(C++20) lazy_split_view is a range
    auto rsplit(std::wstring_view input, std::wstring_view sep, std::size_t max_split)
        -> std::vector<std::wstring>
    {
        return rsplit<decltype(input)::value_type>(input, sep, max_split);
    }

    // TODO(C++20) lazy_split_view is a range
    auto rsplit(std::wstring_view input, wchar_t sep, std::size_t max_split)
        -> std::vector<std::wstring>
    {
        const auto sep_arr = std::array<wchar_t, 2>{ sep, L'\0' };
        return rsplit<decltype(input)::value_type>(input, sep_arr.data(), max_split);
    }

    /*************************************
     *  Implementation of ending_splits  *
     *************************************/

    namespace
    {
        template <typename Char, typename CharOrStrView>
        auto starts_with_split(
            std::basic_string_view<Char> str,
            std::basic_string_view<Char> prefix,
            CharOrStrView sep
        ) -> bool
        {
            auto end = prefix.size();
            const auto sep_size = detail::length(sep);
            const auto str_size = str.size();
            return
                // The substring is found
                starts_with(str, prefix)
                && (
                    // Either it ends at the end
                    (end == str_size)
                    // Or it is found before a separator
                    || ((end <= str_size) && ends_with(str.substr(0, end + sep_size), sep))
                );
        }

        template <typename Char, typename CharOrStrView>
        auto remove_suffix_splits(
            std::basic_string_view<Char> str1,
            std::basic_string_view<Char> str2,
            CharOrStrView sep
        ) -> std::basic_string_view<Char>
        {
            static constexpr auto npos = std::basic_string_view<Char>::npos;

            assert(!str1.empty());
            assert(!str2.empty());
            const auto sep_size = detail::length(sep);
            assert(sep_size > 0);

            auto get_common_candidate = [&](auto split)
            { return str1.substr((split == npos) ? 0 : split + sep_size); };

            auto split1 = str1.rfind(sep);

            // In the case we did not find a match, we try a bigger common part
            while (!starts_with_split(str2, get_common_candidate(split1), sep))
            {
                if ((split1 == npos) || (split1 < sep_size))
                {
                    // No further possibility to find a match, nothing to remove
                    return str1;
                }
                // Add the next split element
                split1 = str1.rfind(sep, split1 - sep_size);
            }

            return str1.substr(0, (split1 == npos) ? 0 : split1);
        }

        template <typename Char, typename CharOrStrView>
        auto concat_dedup_splits_impl(
            std::basic_string_view<Char> str1,
            std::basic_string_view<Char> str2,
            CharOrStrView sep
        ) -> std::basic_string<Char>
        {
            if (str1.empty())
            {
                return std::string(str2);
            }
            if (str2.empty())
            {
                return std::string(str1);
            }
            if (detail::length(sep) < 1)
            {
                throw std::invalid_argument("Cannot split on empty separator");
            }
            auto str1_no_suffix = remove_suffix_splits(str1, str2, sep);
            if (str1_no_suffix.empty())
            {
                return concat(str1_no_suffix, str2);
            }
            return concat(str1_no_suffix, sep, str2);
        }
    }

    auto concat_dedup_splits(std::string_view str1, std::string_view str2, char sep) -> std::string
    {
        return concat_dedup_splits_impl(str1, str2, sep);
    }

    auto concat_dedup_splits(std::string_view str1, std::string_view str2, std::string_view sep)
        -> std::string
    {
        return concat_dedup_splits_impl(str1, str2, sep);
    }

    /*****************************************
     *  Implementation of replace functions  *
     *****************************************/

    namespace
    {
        template <typename Char>
        void replace_all_impl(
            std::basic_string<Char>& data,
            std::basic_string_view<Char> search,
            std::basic_string_view<Char> replace
        )
        {
            if (search.empty())
            {
                return;
            }
            std::size_t pos = data.find(search);
            while (pos != std::string::npos)
            {
                data.replace(pos, search.size(), replace);
                pos = data.find(search, pos + replace.size());
            }
        }
    }

    void replace_all(std::string& data, std::string_view search, std::string_view replace)
    {
        replace_all_impl(data, search, replace);
    }

    void replace_all(std::wstring& data, std::wstring_view search, std::wstring_view replace)
    {
        replace_all_impl(data, search, replace);
    }

    /**************************************
     *  Implementation of join functions  *
     **************************************/

    namespace detail
    {
        auto length(const char* s) -> std::size_t
        {
            return std::strlen(s);
        }

        auto length(const wchar_t* s) -> std::size_t
        {
            return std::wcslen(s);
        }

        auto length(const char /*c*/) -> std::size_t
        {
            return 1;
        }

        auto length(const wchar_t /*c*/) -> std::size_t
        {
            return 1;
        }

    }
}
