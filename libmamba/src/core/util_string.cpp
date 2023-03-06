// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/util_string.hpp"

#include <cctype>
#include <cwctype>
#include <exception>
#include <type_traits>

namespace mamba
{
    /***************************************************
     *  Implementation of to_lower to_upper functions  *
     ***************************************************/

    char to_lower(char c)
    {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    wchar_t to_lower(wchar_t c)
    {
        return static_cast<wchar_t>(std::tolower(c));
    }

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

    char to_upper(char c)
    {
        return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    wchar_t to_upper(wchar_t c)
    {
        return static_cast<wchar_t>(std::toupper(c));
    }

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
    bool ends_with(std::string_view str, std::string_view suffix)
    {
        return str.size() >= suffix.size()
               && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
    }

    // TODO(C++20) This is a method of string_view
    bool starts_with(std::string_view str, std::string_view prefix)
    {
        return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
    }

    /*********************************************
     *  Implementation of starts_with functions  *
     *********************************************/

    template bool any_starts_with(const std::vector<std::string>&, std::string_view);
    template bool any_starts_with(const std::vector<std::string_view>&, std::string_view);

    template bool starts_with_any(std::string_view, const std::vector<std::string>&);
    template bool starts_with_any(std::string_view, const std::vector<std::string_view>&);

    /***************************************
     *  Implementation of strip functions  *
     ***************************************/

    namespace
    {
        constexpr const char* WHITESPACES(" \r\n\t\f\v");
        constexpr const wchar_t* WHITESPACES_WSTR(L" \r\n\t\f\v");

        template <typename Char, typename CharOrStrView>
        std::basic_string_view<Char>
        lstrip_impl(std::basic_string_view<Char> input, CharOrStrView chars)
        {
            std::size_t const start = input.find_first_not_of(chars);
            return (start == std::string::npos) ? std::basic_string_view<Char>{}
                                                : input.substr(start);
        }
    }

    std::string_view lstrip(std::string_view input, char c)
    {
        return lstrip_impl(input, c);
    }
    std::wstring_view lstrip(std::wstring_view input, wchar_t c)
    {
        return lstrip_impl(input, c);
    }
    std::string_view lstrip(std::string_view input, std::string_view chars)
    {
        return lstrip_impl(input, chars);
    }
    std::wstring_view lstrip(std::wstring_view input, std::wstring_view chars)
    {
        return lstrip_impl(input, chars);
    }
    std::string_view lstrip(std::string_view input)
    {
        return lstrip(input, WHITESPACES);
    }
    std::wstring_view lstrip(std::wstring_view input)
    {
        return lstrip(input, WHITESPACES_WSTR);
    }

    namespace
    {
        template <typename Char, typename CharOrStrView>
        std::basic_string_view<Char>
        rstrip_impl(std::basic_string_view<Char> input, CharOrStrView chars)
        {
            std::size_t const end = input.find_last_not_of(chars);
            return (end == std::string::npos) ? std::basic_string_view<Char>{}
                                              : input.substr(0, end + 1);
        }
    }

    std::string_view rstrip(std::string_view input, char c)
    {
        return rstrip_impl(input, c);
    }
    std::wstring_view rstrip(std::wstring_view input, wchar_t c)
    {
        return rstrip_impl(input, c);
    }
    std::string_view rstrip(std::string_view input, std::string_view chars)
    {
        return rstrip_impl(input, chars);
    }
    std::wstring_view rstrip(std::wstring_view input, std::wstring_view chars)
    {
        return rstrip_impl(input, chars);
    }
    std::string_view rstrip(std::string_view input)
    {
        return rstrip(input, WHITESPACES);
    }
    std::wstring_view rstrip(std::wstring_view input)
    {
        return rstrip(input, WHITESPACES_WSTR);
    }

    namespace
    {
        template <typename Char, typename CharOrStrView>
        std::basic_string_view<Char>
        strip_impl(std::basic_string_view<Char> input, CharOrStrView chars)
        {
            std::size_t const start = input.find_first_not_of(chars);
            if (start == std::basic_string_view<Char>::npos)
            {
                return {};
            }
            std::size_t const stop = input.find_last_not_of(chars) + 1;
            std::size_t const length = stop - start;
            return (length == 0) ? std::basic_string_view<Char>{} : input.substr(start, length);
        }
    }

    std::string_view strip(std::string_view input, char c)
    {
        return strip_impl(input, c);
    }
    std::wstring_view strip(std::wstring_view input, wchar_t c)
    {
        return strip_impl(input, c);
    }
    std::string_view strip(std::string_view input, std::string_view chars)
    {
        return strip_impl(input, chars);
    }
    std::wstring_view strip(std::wstring_view input, std::wstring_view chars)
    {
        return strip_impl(input, chars);
    }
    std::string_view strip(std::string_view input)
    {
        return strip(input, WHITESPACES);
    }
    std::wstring_view strip(std::wstring_view input)
    {
        return strip(input, WHITESPACES_WSTR);
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

            std::size_t const len = input.size();
            std::size_t const n = sep.size();
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

            std::size_t const len = input.size();
            std::size_t const n = sep.size();
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

    std::vector<std::string>
    split(std::string_view input, std::string_view sep, std::size_t max_split)
    {
        return split<decltype(input)::value_type>(input, sep, max_split);
    }

    std::vector<std::wstring>
    split(std::wstring_view input, std::wstring_view sep, std::size_t max_split)
    {
        return split<decltype(input)::value_type>(input, sep, max_split);
    }

    std::vector<std::string>
    rsplit(std::string_view input, std::string_view sep, std::size_t max_split)
    {
        return rsplit<decltype(input)::value_type>(input, sep, max_split);
    }

    std::vector<std::wstring>
    rsplit(std::wstring_view input, std::wstring_view sep, std::size_t max_split)
    {
        return rsplit<decltype(input)::value_type>(input, sep, max_split);
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
        std::size_t size(const char* s)
        {
            return std::strlen(s);
        }
        std::size_t size(const wchar_t* s)
        {
            return std::wcslen(s);
        }
        std::size_t size(const char /*c*/)
        {
            return 1;
        }
    }
}
