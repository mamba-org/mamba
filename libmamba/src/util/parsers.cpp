// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <cassert>

#include "mamba/util/conditional.hpp"
#include "mamba/util/parsers.hpp"
#include "mamba/util/string.hpp"

namespace mamba::util
{
    auto find_matching_parentheses_idx(  //
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
            err = ParseError::NotFound;
            return { npos, npos };
        }
        depth += int(text[start] == open) - int(text[start] == close);
        if (depth < 0)
        {
            err = ParseError::InvalidInput;
            return {};
        }

        auto end = text.find_first_of(open_or_close_str, start + 1);
        while (end != npos)
        {
            depth += int(text[end] == open) - int(text[end] == close);
            if (depth == 0)
            {
                return { start, end + 1 };
            }
            if (depth < 0)
            {
                err = ParseError::InvalidInput;
                return {};
            }
            end = text.find_first_of(open_or_close_str, end + 1);
        }
        err = ParseError::InvalidInput;
        return {};
    }

    auto find_matching_parentheses_idx(  //
        std::string_view text,
        char open,
        char close
    ) noexcept -> tl::expected<std::pair<std::size_t, std::size_t>, ParseError>
    {
        auto err = ParseError::Ok;
        auto out = find_matching_parentheses_idx(text, err, open, close);
        if (err != ParseError::Ok)
        {
            return tl::make_unexpected(err);
        }
        return { out };
    }

    auto find_matching_parentheses_str(  //
        std::string_view text,
        ParseError& err,
        char open,
        char close
    ) noexcept -> std::string_view
    {
        const auto [start, end] = find_matching_parentheses_idx(text, err, open, close);
        return (start == std::string_view::npos) ? "" : text.substr(start, end);
    }

    auto find_matching_parentheses_str(  //
        std::string_view text,
        char open,
        char close
    ) noexcept -> tl::expected<std::string_view, ParseError>
    {
        auto err = ParseError::Ok;
        const auto [start, end] = find_matching_parentheses_idx(text, err, open, close);
        if (err != ParseError::Ok)
        {
            return tl::make_unexpected(err);
        }
        return { (start == std::string_view::npos) ? "" : text.substr(start, end) };
    }

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
        static constexpr auto npos = std::string_view::npos;

        if (val.empty())
        {
            err = ParseError::InvalidInput;
            return npos;
        }

        const auto tokens = std::array{ val.front(), open, close };
        const auto tokens_str = std::string_view(tokens.data(), tokens.size());

        int depth = 0;
        auto first_val_pos = npos;
        auto pos = text.find_first_of(tokens_str);
        while (pos != npos)
        {
            depth = if_else(
                (open == close) && (text[pos] == open),
                if_else(depth > 0, 0, 1),  // swap 0 and 1
                depth + int(text[pos] == open) - int(text[pos] == close)
            );
            // Set error but sill try to find the value
            err = if_else(depth < 0, ParseError::InvalidInput, err);
            const bool match = starts_with(text.substr(pos), val);
            first_val_pos = if_else(match && (pos == npos), pos, first_val_pos);
            if ((depth == 0) && match)
            {
                return pos;
            }
            pos = text.find_first_of(tokens_str, pos + 1);
        }
        err = if_else(
            err == ParseError::Ok,
            if_else(depth == 0, ParseError::NotFound, ParseError::InvalidInput),
            err
        );
        return first_val_pos;
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
