// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <cctype>
#include <cwchar>
#include <cwctype>
#include <stdexcept>

#include <stddef.h>

#include "mamba/util/string.hpp"

namespace mamba::util
{
    /****************************************
     *  Implementation of cctype functions  *
     ****************************************/

    bool is_control(char c)
    {
        return std::iscntrl(static_cast<unsigned char>(c)) != 0;
    }

    bool is_control(wchar_t c)
    {
        return std::iswcntrl(static_cast<wint_t>(c)) != 0;
    }

    bool is_print(char c)
    {
        return std::isprint(static_cast<unsigned char>(c)) != 0;
    }

    bool is_print(wchar_t c)
    {
        return std::iswprint(static_cast<wint_t>(c)) != 0;
    }

    bool is_space(char c)
    {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    }

    bool is_space(wchar_t c)
    {
        return std::iswspace(static_cast<wint_t>(c)) != 0;
    }

    bool is_blank(char c)
    {
        return std::isblank(static_cast<unsigned char>(c)) != 0;
    }

    bool is_blank(wchar_t c)
    {
        return std::iswblank(static_cast<wint_t>(c)) != 0;
    }

    bool is_graphic(char c)
    {
        return std::isgraph(static_cast<unsigned char>(c)) != 0;
    }

    bool is_graphic(wchar_t c)
    {
        return std::iswgraph(static_cast<wint_t>(c)) != 0;
    }

    bool is_digit(char c)
    {
        return std::isdigit(static_cast<unsigned char>(c)) != 0;
    }

    bool is_digit(wchar_t c)
    {
        return std::iswdigit(static_cast<wint_t>(c)) != 0;
    }

    bool is_punct(char c)
    {
        return std::ispunct(static_cast<unsigned char>(c)) != 0;
    }

    bool is_punct(wchar_t c)
    {
        return std::iswpunct(static_cast<wint_t>(c)) != 0;
    }

    bool is_alpha(char c)
    {
        return std::isalpha(static_cast<unsigned char>(c)) != 0;
    }

    bool is_alpha(wchar_t c)
    {
        return std::iswalpha(static_cast<wint_t>(c)) != 0;
    }

    bool is_alphanum(char c)
    {
        return std::isalnum(static_cast<unsigned char>(c)) != 0;
    }

    bool is_alphanum(wchar_t c)
    {
        return std::iswalnum(static_cast<wint_t>(c)) != 0;
    }

    bool is_lower(char c)
    {
        return std::islower(static_cast<unsigned char>(c)) != 0;
    }

    bool is_lower(wchar_t c)
    {
        return std::iswlower(static_cast<wint_t>(c)) != 0;
    }

    bool is_upper(char c)
    {
        return std::isupper(static_cast<unsigned char>(c)) != 0;
    }

    bool is_upper(wchar_t c)
    {
        return std::iswupper(static_cast<wint_t>(c)) != 0;
    }

    char to_lower(char c)
    {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    wchar_t to_lower(wchar_t c)
    {
        return static_cast<wchar_t>(std::towlower(static_cast<wint_t>(c)));
    }

    char to_upper(char c)
    {
        return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    wchar_t to_upper(wchar_t c)
    {
        return static_cast<wchar_t>(std::towupper(static_cast<wint_t>(c)));
    }


    /***************************************************
     *  Implementation of to_lower to_upper functions  *
     ***************************************************/

    namespace
    {
        template <typename Char>
        std::basic_string<Char> to_lower_impl(std::basic_string_view<Char> str)
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

    std::string to_lower(std::string_view str)
    {
        return to_lower_impl(str);
    }

    std::wstring to_lower(std::wstring_view str)
    {
        return to_lower_impl(str);
    }

    template <typename Char>
    std::basic_string<Char> to_lower(std::basic_string<Char>&& str)
    {
        std::transform(str.cbegin(), str.cend(), str.begin(), [](auto c) { return to_lower(c); });
        return str;
    }

    template std::string to_lower(std::string&& str);
    template std::wstring to_lower(std::wstring&& str);

    namespace
    {
        template <typename Char>
        std::basic_string<Char> to_upper_impl(std::basic_string_view<Char> str)
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

    std::string to_upper(std::string_view str)
    {
        return to_upper_impl(str);
    }

    std::wstring to_upper(std::wstring_view str)
    {
        return to_upper_impl(str);
    }

    template <typename Char>
    std::basic_string<Char> to_upper(std::basic_string<Char>&& str)
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
    bool contains(std::string_view str, std::string_view sub_str)
    {
        return str.find(sub_str) != std::string::npos;
    }

    // TODO(C++20) This is a method of string_view
    bool contains(std::string_view str, char c)
    {
        return str.find(c) != std::string::npos;
    }

    bool contains(char c1, char c2)
    {
        return c1 == c2;
    }

    // TODO(C++20) This is a method of string_view
    bool ends_with(std::string_view str, std::string_view suffix)
    {
        return str.size() >= suffix.size()
               && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
    }

    bool ends_with(std::string_view str, std::string_view::value_type c)
    {
        return (!str.empty()) && (str.back() == c);
    }

    // TODO(C++20) This is a method of string_view
    bool starts_with(std::string_view str, std::string_view prefix)
    {
        return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
    }

    // TODO(C++20) This is a method of string_view
    bool starts_with(std::string_view str, std::string_view::value_type c)
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

    std::array<std::string_view, 2> split_prefix(std::string_view str, std::string_view prefix)
    {
        if (starts_with(str, prefix))
        {
            return { str.substr(0, prefix.size()), str.substr(prefix.size()) };
        }
        return { std::string_view(), str };
    }

    std::array<std::string_view, 2> split_prefix(std::string_view str, std::string_view::value_type c)
    {
        if (starts_with(str, c))
        {
            return { str.substr(0, 1), str.substr(1) };
        }
        return { std::string_view(), str };
    }

    std::string_view remove_prefix(std::string_view str, std::string_view prefix)
    {
        return std::get<1>(split_prefix(str, prefix));
    }

    std::string_view remove_prefix(std::string_view str, std::string_view::value_type c)
    {
        return std::get<1>(split_prefix(str, c));
    }

    std::array<std::string_view, 2> split_suffix(std::string_view str, std::string_view suffix)
    {
        if (ends_with(str, suffix))
        {
            auto suffix_pos = str.size() - suffix.size();
            return { str.substr(0, suffix_pos), str.substr(suffix_pos) };
        }
        return { str, std::string_view() };
    }

    std::array<std::string_view, 2> split_suffix(std::string_view str, std::string_view::value_type c)
    {
        if (ends_with(str, c))
        {
            auto suffix_pos = str.size() - 1;
            return { str.substr(0, suffix_pos), str.substr(suffix_pos) };
        }
        return { str, std::string_view() };
    }

    std::string_view remove_suffix(std::string_view str, std::string_view suffix)
    {
        return std::get<0>(split_suffix(str, suffix));
    }

    std::string_view remove_suffix(std::string_view str, std::string_view::value_type c)
    {
        return std::get<0>(split_suffix(str, c));
    }

    /***************************************
     *  Implementation of strip functions  *
     ***************************************/

    std::string_view lstrip(std::string_view input, char c)
    {
        return lstrip_parts(input, c)[1];
    }

    std::wstring_view lstrip(std::wstring_view input, wchar_t c)
    {
        return lstrip_parts(input, c)[1];
    }

    std::string_view lstrip(std::string_view input, std::string_view chars)
    {
        return lstrip_parts(input, chars)[1];
    }

    std::wstring_view lstrip(std::wstring_view input, std::wstring_view chars)
    {
        return lstrip_parts(input, chars)[1];
    }

    std::string_view lstrip(std::string_view input)
    {
        using Char = decltype(input)::value_type;
        return lstrip_if(input, [](Char c) { return !is_graphic(c); });
    }

    std::wstring_view lstrip(std::wstring_view input)
    {
        using Char = decltype(input)::value_type;
        return lstrip_if(input, [](Char c) { return !is_graphic(c); });
    }

    std::string_view rstrip(std::string_view input, char c)
    {
        return rstrip_parts(input, c)[0];
    }

    std::wstring_view rstrip(std::wstring_view input, wchar_t c)
    {
        return rstrip_parts(input, c)[0];
    }

    std::string_view rstrip(std::string_view input, std::string_view chars)
    {
        return rstrip_parts(input, chars)[0];
    }

    std::wstring_view rstrip(std::wstring_view input, std::wstring_view chars)
    {
        return rstrip_parts(input, chars)[0];
    }

    std::string_view rstrip(std::string_view input)
    {
        using Char = decltype(input)::value_type;
        return rstrip_if(input, [](Char c) { return !is_graphic(c); });
    }
    std::wstring_view rstrip(std::wstring_view input)
    {
        using Char = decltype(input)::value_type;
        return rstrip_if(input, [](Char c) { return !is_graphic(c); });
    }

    std::string_view strip(std::string_view input, char c)
    {
        return strip_parts(input, c)[1];
    }

    std::wstring_view strip(std::wstring_view input, wchar_t c)
    {
        return strip_parts(input, c)[1];
    }

    std::string_view strip(std::string_view input, std::string_view chars)
    {
        return strip_parts(input, chars)[1];
    }

    std::wstring_view strip(std::wstring_view input, std::wstring_view chars)
    {
        return strip_parts(input, chars)[1];
    }

    std::string_view strip(std::string_view input)
    {
        using Char = decltype(input)::value_type;
        return strip_if(input, [](Char c) { return !is_graphic(c); });
    }

    std::wstring_view strip(std::wstring_view input)
    {
        using Char = decltype(input)::value_type;
        return strip_if(input, [](Char c) { return !is_graphic(c); });
    }

    /*********************************************
     *  Implementation of strip_parts functions  *
     *********************************************/

    namespace
    {
        // string_view has a different overload for ``find(char)`` and ``find(string_view)``
        // so we want to leverage that.
        template <typename Char, typename CharOrStrView>
        std::array<std::basic_string_view<Char>, 2>
        lstrip_parts_impl(std::basic_string_view<Char> input, CharOrStrView chars)
        {
            const std::size_t start = input.find_first_not_of(chars);
            if (start == std::basic_string_view<Char>::npos)
            {
                return { input, std::basic_string_view<Char>{} };
            }
            return { input.substr(0, start), input.substr(start) };
        }
    }

    std::array<std::string_view, 2> lstrip_parts(std::string_view input, char c)
    {
        return lstrip_parts_impl(input, c);
    }

    std::array<std::wstring_view, 2> lstrip_parts(std::wstring_view input, wchar_t c)
    {
        return lstrip_parts_impl(input, c);
    }

    std::array<std::string_view, 2> lstrip_parts(std::string_view input, std::string_view chars)
    {
        return lstrip_parts_impl(input, chars);
    }

    std::array<std::wstring_view, 2> lstrip_parts(std::wstring_view input, std::wstring_view chars)
    {
        return lstrip_parts_impl(input, chars);
    }

    namespace
    {
        // string_view has a different overload for ``find(char)`` and ``find(string_view)``
        // so we want to leverage that.
        template <typename Char, typename CharOrStrView>
        std::array<std::basic_string_view<Char>, 2>
        rstrip_parts_impl(std::basic_string_view<Char> input, CharOrStrView chars)
        {
            const std::size_t end = input.find_last_not_of(chars);
            if (end == std::basic_string_view<Char>::npos)
            {
                return { std::basic_string_view<Char>{}, input };
            }
            return { input.substr(0, end + 1), input.substr(end + 1) };
        }
    }

    std::array<std::string_view, 2> rstrip_parts(std::string_view input, char c)
    {
        return rstrip_parts_impl(input, c);
    }

    std::array<std::wstring_view, 2> rstrip_parts(std::wstring_view input, wchar_t c)
    {
        return rstrip_parts_impl(input, c);
    }

    std::array<std::string_view, 2> rstrip_parts(std::string_view input, std::string_view chars)
    {
        return rstrip_parts_impl(input, chars);
    }

    std::array<std::wstring_view, 2> rstrip_parts(std::wstring_view input, std::wstring_view chars)
    {
        return rstrip_parts_impl(input, chars);
    }

    namespace
    {
        // string_view has a different overload for ``find(char)`` and ``find(string_view)``
        // so we want to leverage that.
        template <typename Char, typename CharOrStrView>
        std::array<std::basic_string_view<Char>, 3>
        strip_parts_impl(std::basic_string_view<Char> input, CharOrStrView chars)
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

    std::array<std::string_view, 3> strip_parts(std::string_view input, char c)
    {
        return strip_parts_impl(input, c);
    }
    std::array<std::wstring_view, 3> strip_parts(std::wstring_view input, wchar_t c)
    {
        return strip_parts_impl(input, c);
    }
    std::array<std::string_view, 3> strip_parts(std::string_view input, std::string_view chars)
    {
        return strip_parts_impl(input, chars);
    }
    std::array<std::wstring_view, 3> strip_parts(std::wstring_view input, std::wstring_view chars)
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


    /***************************************
     *  Implementation of split functions  *
     ***************************************/

    namespace
    {
        template <class Char>
        std::vector<std::basic_string<Char>> split(
            const std::basic_string_view<Char> input,
            const std::basic_string_view<Char> sep,
            std::size_t max_split = SIZE_MAX
        )
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
        std::vector<std::basic_string<Char>> rsplit(
            const std::basic_string_view<Char> input,
            const std::basic_string_view<Char> sep,
            std::size_t max_split = SIZE_MAX
        )
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
    std::vector<std::string>
    split(std::string_view input, std::string_view sep, std::size_t max_split)
    {
        return split<decltype(input)::value_type>(input, sep, max_split);
    }

    // TODO(C++20) lazy_split_view is a range
    std::vector<std::string> split(std::string_view input, char sep, std::size_t max_split)
    {
        const auto sep_arr = std::array<char, 2>{ sep, '\0' };
        return split<decltype(input)::value_type>(input, sep_arr.data(), max_split);
    }

    // TODO(C++20) lazy_split_view is a range
    std::vector<std::wstring>
    split(std::wstring_view input, std::wstring_view sep, std::size_t max_split)
    {
        return split<decltype(input)::value_type>(input, sep, max_split);
    }

    // TODO(C++20) lazy_split_view is a range
    std::vector<std::wstring> split(std::wstring_view input, wchar_t sep, std::size_t max_split)
    {
        const auto sep_arr = std::array<wchar_t, 2>{ sep, L'\0' };
        return split<decltype(input)::value_type>(input, sep_arr.data(), max_split);
    }

    // TODO(C++20) lazy_split_view is a range
    std::vector<std::string>
    rsplit(std::string_view input, std::string_view sep, std::size_t max_split)
    {
        return rsplit<decltype(input)::value_type>(input, sep, max_split);
    }

    // TODO(C++20) lazy_split_view is a range
    std::vector<std::string> rsplit(std::string_view input, char sep, std::size_t max_split)
    {
        const auto sep_arr = std::array<char, 2>{ sep, '\0' };
        return rsplit<decltype(input)::value_type>(input, sep_arr.data(), max_split);
    }

    // TODO(C++20) lazy_split_view is a range
    std::vector<std::wstring>
    rsplit(std::wstring_view input, std::wstring_view sep, std::size_t max_split)
    {
        return rsplit<decltype(input)::value_type>(input, sep, max_split);
    }

    // TODO(C++20) lazy_split_view is a range
    std::vector<std::wstring> rsplit(std::wstring_view input, wchar_t sep, std::size_t max_split)
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

    std::string concat_dedup_splits(std::string_view str1, std::string_view str2, char sep)
    {
        return concat_dedup_splits_impl(str1, str2, sep);
    }

    std::string
    concat_dedup_splits(std::string_view str1, std::string_view str2, std::string_view sep)
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
        std::size_t length(const char* s)
        {
            return std::strlen(s);
        }
        std::size_t length(const wchar_t* s)
        {
            return std::wcslen(s);
        }
        std::size_t length(const char /*c*/)
        {
            return 1;
        }
        std::size_t length(const wchar_t /*c*/)
        {
            return 1;
        }

    }
}
