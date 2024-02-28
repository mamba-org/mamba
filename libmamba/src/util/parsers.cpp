// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <cassert>

#include "mamba/util/parsers.hpp"
#include "mamba/util/string.hpp"

namespace mamba::util
{

    /*******************************
     *  find_matching_parentheses  *
     *******************************/

    auto find_matching_parentheses(  //
        std::string_view text,
        ParseError& err,
        char open,
        char close
    ) noexcept -> std::pair<std::size_t, std::size_t>
    {
        static constexpr auto npos = std::string_view::npos;

        const auto open_or_close = std::array{ open, close };
        const auto open_or_close_str = std::string_view(open_or_close.data(), open_or_close.size());

        int depth = 0;

        const auto start = text.find_first_of(open_or_close_str);
        if (start == npos)
        {
            return { npos, npos };
        }

        auto pos = start;
        while (pos != npos)
        {
            depth += if_else(
                open == close,
                if_else(depth > 0, -1, 1),  // Open or close same parentheses
                int(text[pos] == open) - int(text[pos] == close)
            );
            if (depth == 0)
            {
                return { start, pos };
            }
            if (depth < 0)
            {
                err = ParseError::InvalidInput;
                return {};
            }
            pos = text.find_first_of(open_or_close_str, pos + 1);
        }

        err = ParseError::InvalidInput;
        return {};
    }

    auto find_matching_parentheses(  //
        std::string_view text,
        char open,
        char close
    ) noexcept -> tl::expected<std::pair<std::size_t, std::size_t>, ParseError>
    {
        auto err = ParseError::Ok;
        auto out = find_matching_parentheses(text, err, open, close);
        if (err != ParseError::Ok)
        {
            return tl::make_unexpected(err);
        }
        return { out };
    }

    /*****************************
     *  find_not_in_parentheses  *
     *****************************/

    auto find_not_in_parentheses(  //
        std::string_view text,
        char c,
        ParseError& err,
        char open,
        char close
    ) noexcept -> std::size_t
    {
        return find_not_in_parentheses(text, c, err, std::array{ open }, std::array{ close });
    }

    auto find_not_in_parentheses(  //
        std::string_view text,
        char c,
        char open,
        char close
    ) noexcept -> tl::expected<std::size_t, ParseError>
    {
        auto err = ParseError::Ok;
        const auto pos = find_not_in_parentheses(text, c, err, open, close);
        if (err != ParseError::Ok)
        {
            return tl::make_unexpected(err);
        }
        return { pos };
    }

    auto find_not_in_parentheses(  //
        std::string_view text,
        std::string_view val,
        ParseError& err,
        char open,
        char close
    ) noexcept -> std::size_t
    {
        return detail_parsers::find_not_in_parentheses_impl(
            text,
            val,
            err,
            std::array{ open },
            std::array{ close },
            detail_parsers::FindParenthesesSearcher()
        );
    }

    auto find_not_in_parentheses(  //
        std::string_view text,
        std::string_view val,
        char open,
        char close
    ) noexcept -> tl::expected<std::size_t, ParseError>
    {
        auto err = ParseError::Ok;
        const auto pos = find_not_in_parentheses(text, val, err, open, close);
        if (err != ParseError::Ok)
        {
            return tl::make_unexpected(err);
        }
        return { pos };
    }

    /******************************
     *  rfind_not_in_parentheses  *
     ******************************/

    auto rfind_not_in_parentheses(  //
        std::string_view text,
        char c,
        ParseError& err,
        char open,
        char close
    ) noexcept -> std::size_t
    {
        return rfind_not_in_parentheses(text, c, err, std::array{ open }, std::array{ close });
    }

    auto rfind_not_in_parentheses(  //
        std::string_view text,
        char c,
        char open,
        char close
    ) noexcept -> tl::expected<std::size_t, ParseError>
    {
        auto err = ParseError::Ok;
        const auto pos = rfind_not_in_parentheses(text, c, err, open, close);
        if (err != ParseError::Ok)
        {
            return tl::make_unexpected(err);
        }
        return { pos };
    }

    auto rfind_not_in_parentheses(  //
        std::string_view text,
        std::string_view val,
        ParseError& err,
        char open,
        char close
    ) noexcept -> std::size_t
    {
        return detail_parsers::find_not_in_parentheses_impl(
            text,
            val,
            err,
            std::array{ close },  // swaped
            std::array{ open },
            detail_parsers::RFindParenthesesSearcher()
        );
    }

    auto rfind_not_in_parentheses(  //
        std::string_view text,
        std::string_view val,
        char open,
        char close
    ) noexcept -> tl::expected<std::size_t, ParseError>
    {
        auto err = ParseError::Ok;
        const auto pos = rfind_not_in_parentheses(text, val, err, open, close);
        if (err != ParseError::Ok)
        {
            return tl::make_unexpected(err);
        }
        return { pos };
    }

    /**********
     *  glob  *
     **********/

    namespace
    {
        auto glob_match_impl(std::string_view pattern, std::string_view str, char glob) -> bool
        {
            static constexpr auto npos = std::string_view::npos;

            assert(starts_with(pattern, glob));
            pattern = lstrip(pattern, glob);  // Drop leading '*'
            if (pattern.empty())              // input pattern was "*"
            {
                return true;
            }

            const auto next_glob = pattern.find(glob);
            const auto word = pattern.substr(0, next_glob);
            if (const auto wpos = str.find(word); wpos != npos)
            {
                if (next_glob == npos)
                {
                    // Return ends_with(str, word);
                    return (wpos + word.size()) == str.size();
                }
                return glob_match_impl(pattern.substr(next_glob), str.substr(wpos + word.size()), glob);
            }
            return false;
        }
    }

    auto glob_match(std::string_view pattern, std::string_view str, char glob) -> bool
    {
        static constexpr auto npos = std::string_view::npos;

        if (starts_with(pattern, glob))
        {
            return glob_match_impl(pattern, str, glob);
        }
        if (const auto next_glob = pattern.find(glob); next_glob != npos)
        {
            const auto word = pattern.substr(0, next_glob);
            return starts_with(str, word)
                   && glob_match_impl(pattern.substr(next_glob), remove_prefix(str, word), glob);
            ;
        }
        return str == pattern;
    }
}
