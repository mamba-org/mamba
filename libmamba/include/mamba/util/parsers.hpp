// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_PARSERS_HPP
#define MAMBA_UTIL_PARSERS_HPP

#include <string_view>
#include <utility>

#include <tl/expected.hpp>

#include "mamba/util/conditional.hpp"
#include "mamba/util/string.hpp"

namespace mamba::util
{

    enum struct ParseError : bool
    {
        Ok = true,
        InvalidInput = false,
    };

    /**
     * Find the first opening parenthesis and its matching pair.
     *
     * Correctly matches parentheses together so that inner parentheses pairs are skipped.
     * Open and closing pairs don't need to be different.
     * If an error is encountered, @p err is modified to contain the error, otherwise it is left
     * as it is.
     */
    auto find_matching_parentheses(  //
        std::string_view text,
        ParseError& err,
        char open = '(',
        char close = ')'
    ) noexcept -> std::pair<std::size_t, std::size_t>;

    [[nodiscard]] auto find_matching_parentheses(  //
        std::string_view text,
        char open = '(',
        char close = ')'
    ) noexcept -> tl::expected<std::pair<std::size_t, std::size_t>, ParseError>;

    template <std::size_t P>
    auto find_matching_parentheses(  //
        std::string_view text,
        ParseError& err,
        const std::array<char, P>& open = { '(', '[' },
        const std::array<char, P>& close = { ')', ']' }
    ) noexcept -> std::pair<std::size_t, std::size_t>;

    template <std::size_t P>
    [[nodiscard]] auto find_matching_parentheses(  //
        std::string_view text,
        const std::array<char, P>& open = { '(', '[' },
        const std::array<char, P>& close = { ')', ']' }
    ) noexcept -> tl::expected<std::pair<std::size_t, std::size_t>, ParseError>;

    /**
     * Find the last closing parenthesese and its matching pair.
     *
     * Correctly matches parenteses together so that inner parentheses pairs are skipped.
     * Open and closing pairs don't need to be different.
     * If an error is encountered, @p err is modified to contain the error, otherwise it is left
     * as it is.
     */
    auto rfind_matching_parentheses(  //
        std::string_view text,
        ParseError& err,
        char open = '(',
        char close = ')'
    ) noexcept -> std::pair<std::size_t, std::size_t>;

    [[nodiscard]] auto rfind_matching_parentheses(  //
        std::string_view text,
        char open = '(',
        char close = ')'
    ) noexcept -> tl::expected<std::pair<std::size_t, std::size_t>, ParseError>;

    template <std::size_t P>
    auto rfind_matching_parentheses(  //
        std::string_view text,
        ParseError& err,
        const std::array<char, P>& open = { '(', '[' },
        const std::array<char, P>& close = { ')', ']' }
    ) noexcept -> std::pair<std::size_t, std::size_t>;

    template <std::size_t P>
    [[nodiscard]] auto rfind_matching_parentheses(  //
        std::string_view text,
        const std::array<char, P>& open = { '(', '[' },
        const std::array<char, P>& close = { ')', ']' }
    ) noexcept -> tl::expected<std::pair<std::size_t, std::size_t>, ParseError>;

    /**
     * Find a character or string, except in matching parentheses pairs.
     *
     * Find the first occurrence of the given character, except if such character is inside a valid
     * pair of parentheses.
     * Open and closing pairs don't need to be different.
     * If not found, ``std::string_view::npos`` is returned but no error is set as this is not
     * considered an error.
     * Due to a greedy approach, the function may not be able to detect all errors, but will be
     * correct when parentheses are correctly matched.
     */
    auto find_not_in_parentheses(  //
        std::string_view text,
        char c,
        ParseError& err,
        char open = '(',
        char close = ')'
    ) noexcept -> std::size_t;

    [[nodiscard]] auto find_not_in_parentheses(  //
        std::string_view text,
        char c,
        char open = '(',
        char close = ')'
    ) noexcept -> tl::expected<std::size_t, ParseError>;

    template <std::size_t P>
    auto find_not_in_parentheses(
        std::string_view text,
        char c,
        ParseError& err,
        const std::array<char, P>& open = { '(', '[' },
        const std::array<char, P>& close = { ')', ']' }
    ) noexcept -> std::size_t;

    template <std::size_t P>
    [[nodiscard]] auto find_not_in_parentheses(
        std::string_view text,
        char c,
        const std::array<char, P>& open = { '(', '[' },
        const std::array<char, P>& close = { ')', ']' }
    ) noexcept -> tl::expected<std::size_t, ParseError>;

    auto find_not_in_parentheses(  //
        std::string_view text,
        std::string_view val,
        ParseError& err,
        char open = '(',
        char close = ')'
    ) noexcept -> std::size_t;

    [[nodiscard]] auto find_not_in_parentheses(  //
        std::string_view text,
        std::string_view val,
        char open = '(',
        char close = ')'
    ) noexcept -> tl::expected<std::size_t, ParseError>;

    template <std::size_t P>
    auto find_not_in_parentheses(
        std::string_view text,
        std::string_view val,
        ParseError& err,
        const std::array<char, P>& open = { '(', '[' },
        const std::array<char, P>& close = { ')', ']' }
    ) noexcept -> std::size_t;

    template <std::size_t P>
    [[nodiscard]] auto find_not_in_parentheses(
        std::string_view text,
        std::string_view val,
        const std::array<char, P>& open = { '(', '[' },
        const std::array<char, P>& close = { ')', ']' }
    ) noexcept -> tl::expected<std::size_t, ParseError>;

    /**
     * Find the last character or string, except in matching parentheses pairs.
     *
     * Find the last occurrence of the given character, except if such character is inside a valid
     * pair of parentheses.
     * Open and closing pairs don't need to be different.
     * If not found, ``std::string_view::npos`` is returned but no error is set as this is not
     * considered an error.
     * Due to a greedy approach, the function may not be able to detect all errors, but will be
     * correct when parentheses are correctly matched.
     */
    auto rfind_not_in_parentheses(  //
        std::string_view text,
        char c,
        ParseError& err,
        char open = '(',
        char close = ')'
    ) noexcept -> std::size_t;

    [[nodiscard]] auto rfind_not_in_parentheses(  //
        std::string_view text,
        char c,
        char open = '(',
        char close = ')'
    ) noexcept -> tl::expected<std::size_t, ParseError>;

    template <std::size_t P>
    auto rfind_not_in_parentheses(
        std::string_view text,
        char c,
        ParseError& err,
        const std::array<char, P>& open = { '(', '[' },
        const std::array<char, P>& close = { ')', ']' }
    ) noexcept -> std::size_t;

    template <std::size_t P>
    [[nodiscard]] auto rfind_not_in_parentheses(
        std::string_view text,
        char c,
        const std::array<char, P>& open = { '(', '[' },
        const std::array<char, P>& close = { ')', ']' }
    ) noexcept -> tl::expected<std::size_t, ParseError>;

    auto rfind_not_in_parentheses(  //
        std::string_view text,
        std::string_view val,
        ParseError& err,
        char open = '(',
        char close = ')'
    ) noexcept -> std::size_t;

    [[nodiscard]] auto rfind_not_in_parentheses(  //
        std::string_view text,
        std::string_view val,
        char open = '(',
        char close = ')'
    ) noexcept -> tl::expected<std::size_t, ParseError>;

    template <std::size_t P>
    auto rfind_not_in_parentheses(
        std::string_view text,
        std::string_view val,
        ParseError& err,
        const std::array<char, P>& open = { '(', '[' },
        const std::array<char, P>& close = { ')', ']' }
    ) noexcept -> std::size_t;

    template <std::size_t P>
    [[nodiscard]] auto rfind_not_in_parentheses(
        std::string_view text,
        std::string_view val,
        const std::array<char, P>& open = { '(', '[' },
        const std::array<char, P>& close = { ')', ']' }
    ) noexcept -> tl::expected<std::size_t, ParseError>;

    /**
     * Test whether the glob pattern @p pattern matches the string @p str.
     */
    [[nodiscard]] auto glob_match(std::string_view pattern, std::string_view str, char glob = '*')
        -> bool;

    /********************
     *  Implementation  *
     ********************/

    namespace detail_parsers
    {
        template <typename T, typename... Arr>
        constexpr auto concat_array(const Arr&... arrs)
        {
            auto out = std::array<T, (... + sizeof(arrs))>{};
            std::size_t out_idx = 0;
            auto copy_one = [&](const auto& a)
            {
                for (const auto& x : a)
                {
                    out[out_idx++] = x;
                }
            };
            (copy_one(arrs), ...);
            return out;
        }

        template <typename T, std::size_t N>
        constexpr auto find(const std::array<T, N>& arr, const T& val) -> std::size_t
        {
            std::size_t pos = N;
            for (std::size_t i = 0; i < N; ++i)
            {
                const bool found = arr[i] == val;
                pos = found ? i : pos;
            }
            return pos;
        }

        inline auto front(char c) -> char
        {
            return c;
        }

        inline auto front(std::string_view str) -> char
        {
            return str.front();
        }

        inline auto empty(char) -> bool
        {
            return false;
        }

        inline auto empty(std::string_view str) -> bool
        {
            return str.empty();
        }

        struct FindParenthesesSearcher
        {
            auto find_first(std::string_view text, std::string_view token_str)
            {
                return text.find_first_of(token_str);
            }

            auto find_next(std::string_view text, std::string_view token_str, std::size_t pos)
            {
                return text.find_first_of(token_str, pos + 1);
            }
        };

        struct RFindParenthesesSearcher
        {
            auto find_first(std::string_view text, std::string_view token_str)
            {
                return text.find_last_of(token_str);
            }

            auto find_next(std::string_view text, std::string_view token_str, std::size_t pos)
            {
                return (pos == 0) ? text.npos : text.find_last_of(token_str, pos - 1);
            }
        };

        template <std::size_t P, typename Searcher>
        auto find_matching_parentheses_impl(
            std::string_view text,
            ParseError& err,
            const std::array<char, P>& open,
            const std::array<char, P>& close,
            Searcher&& searcher
        ) noexcept -> std::pair<std::size_t, std::size_t>
        {
            // TODO(C++20): After allocating tokens and depths here, call an impl function using
            // std::span defined in .cpp
            static constexpr auto sv_npos = std::string_view::npos;

            const auto tokens = detail_parsers::concat_array<char>(open, close);
            const auto tokens_str = std::string_view(tokens.data(), tokens.size());

            auto depths = std::array<int, P + 1>{};  // Plus one for branchless depths code

            const auto start = searcher.find_first(text, tokens_str);
            if (start == sv_npos)
            {
                return { sv_npos, sv_npos };
            }

            auto pos = start;
            while (pos != sv_npos)
            {
                // Change depth of corresponding open/close pair, writing in index P for
                // the one not matching.
                const auto open_depth_idx = detail_parsers::find(open, text[pos]);
                const auto close_depth_idx = detail_parsers::find(close, text[pos]);
                depths[open_depth_idx] += int(open_depth_idx < open.size());
                depths[close_depth_idx] -= int(close_depth_idx < open.size());
                // When open and close are the same character, depth did not change so we make
                // a swap operation
                depths[open_depth_idx] = if_else(
                    open_depth_idx == close_depth_idx,
                    if_else(depths[open_depth_idx] > 0, 0, 1),  // swap 0 and 1
                    depths[open_depth_idx]
                );
                depths[P] = 0;

                // All parentheses are properly closed, we found the matching one.
                if (depths == decltype(depths){})
                {
                    return { start, pos };
                }

                // Any negative depth means mismatched parentheses
                for (auto d : depths)
                {
                    err = if_else(d < 0, ParseError::InvalidInput, err);
                }

                pos = searcher.find_next(text, tokens_str, pos);
            }

            err = ParseError::InvalidInput;
            return { start, sv_npos };
        }

        template <std::size_t P, typename Str, typename Searcher>
        auto find_not_in_parentheses_impl(
            std::string_view text,
            const Str& val,
            ParseError& err,
            const std::array<char, P>& open,
            const std::array<char, P>& close,
            Searcher&& searcher
        ) noexcept -> std::size_t
        {
            // TODO(C++20): After allocating tokens and depths here, call an impl function using
            // std::span defined in .cpp
            static constexpr auto sv_npos = std::string_view::npos;

            if (detail_parsers::empty(val))
            {
                err = ParseError::InvalidInput;
                return sv_npos;
            }

            const auto tokens = detail_parsers::concat_array<char>(
                std::array{ detail_parsers::front(val) },
                open,
                close
            );
            const auto tokens_str = std::string_view(tokens.data(), tokens.size());

            auto depths = std::array<int, P + 1>{};  // last for easy branchless access
            auto first_val_pos = sv_npos;
            auto pos = searcher.find_first(text, tokens_str);
            while (pos != sv_npos)
            {
                const auto open_pos = detail_parsers::find(open, text[pos]);
                const auto close_pos = detail_parsers::find(close, text[pos]);
                depths[open_pos] += int(open_pos < open.size());
                depths[close_pos] -= int(close_pos < open.size());
                depths[open_pos] = if_else(
                    open_pos == close_pos,
                    if_else(depths[open_pos] > 0, 0, 1),  // swap 0 and 1
                    depths[open_pos]
                );
                depths[P] = 0;

                for (auto d : depths)
                {
                    err = if_else(d < 0, ParseError::InvalidInput, err);
                }
                const bool match = starts_with(text.substr(pos), val);
                first_val_pos = if_else(match && (pos == sv_npos), pos, first_val_pos);
                if (match && (depths == decltype(depths){}))
                {
                    return pos;
                }
                pos = searcher.find_next(text, tokens_str, pos);
            }
            // Check if all parentheses are properly closed
            if (depths != decltype(depths){})
            {
                err = ParseError::InvalidInput;
                return first_val_pos;
            }
            return sv_npos;  // not found
        }
    }

    /*******************************
     *  find_matching_parentheses  *
     *******************************/

    template <std::size_t P>
    auto find_matching_parentheses(  //
        std::string_view text,
        ParseError& err,
        const std::array<char, P>& open,
        const std::array<char, P>& close
    ) noexcept -> std::pair<std::size_t, std::size_t>
    {
        return detail_parsers::find_matching_parentheses_impl(
            text,
            err,
            open,
            close,
            detail_parsers::FindParenthesesSearcher()
        );
    }

    template <std::size_t P>
    [[nodiscard]] auto find_matching_parentheses(  //
        std::string_view text,
        const std::array<char, P>& open,
        const std::array<char, P>& close
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

    /********************************
     *  rfind_matching_parentheses  *
     ********************************/

    template <std::size_t P>
    auto rfind_matching_parentheses(  //
        std::string_view text,
        ParseError& err,
        const std::array<char, P>& open,
        const std::array<char, P>& close
    ) noexcept -> std::pair<std::size_t, std::size_t>
    {
        auto [last, first] = detail_parsers::find_matching_parentheses_impl(
            text,
            err,
            close,  // swapped
            open,
            detail_parsers::RFindParenthesesSearcher()
        );
        return { first, last };
    }

    template <std::size_t P>
    [[nodiscard]] auto rfind_matching_parentheses(  //
        std::string_view text,
        const std::array<char, P>& open,
        const std::array<char, P>& close
    ) noexcept -> tl::expected<std::pair<std::size_t, std::size_t>, ParseError>
    {
        auto err = ParseError::Ok;
        auto out = rfind_matching_parentheses(text, err, open, close);
        if (err != ParseError::Ok)
        {
            return tl::make_unexpected(err);
        }
        return { out };
    }

    /*****************************
     *  find_not_in_parentheses  *
     *****************************/

    template <std::size_t P>
    auto find_not_in_parentheses(
        std::string_view text,
        char c,
        ParseError& err,
        const std::array<char, P>& open,
        const std::array<char, P>& close
    ) noexcept -> std::size_t
    {
        return detail_parsers::find_not_in_parentheses_impl(
            text,
            c,
            err,
            open,
            close,
            detail_parsers::FindParenthesesSearcher()
        );
    }

    template <std::size_t P>
    auto find_not_in_parentheses(
        std::string_view text,
        char c,
        const std::array<char, P>& open,
        const std::array<char, P>& close
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

    template <std::size_t P>
    auto find_not_in_parentheses(
        std::string_view text,
        std::string_view val,
        ParseError& err,
        const std::array<char, P>& open,
        const std::array<char, P>& close
    ) noexcept -> std::size_t
    {
        return detail_parsers::find_not_in_parentheses_impl(
            text,
            val,
            err,
            open,
            close,
            detail_parsers::FindParenthesesSearcher()
        );
    }

    template <std::size_t P>
    [[nodiscard]] auto find_not_in_parentheses(
        std::string_view text,
        std::string_view val,
        const std::array<char, P>& open,
        const std::array<char, P>& close
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

    template <std::size_t P>
    auto rfind_not_in_parentheses(
        std::string_view text,
        char c,
        ParseError& err,
        const std::array<char, P>& open,
        const std::array<char, P>& close
    ) noexcept -> std::size_t
    {
        return detail_parsers::find_not_in_parentheses_impl(
            text,
            c,
            err,
            close,  // swapped
            open,
            detail_parsers::RFindParenthesesSearcher()
        );
    }

    template <std::size_t P>
    auto rfind_not_in_parentheses(
        std::string_view text,
        char c,
        const std::array<char, P>& open,
        const std::array<char, P>& close
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

    template <std::size_t P>
    auto rfind_not_in_parentheses(
        std::string_view text,
        std::string_view val,
        ParseError& err,
        const std::array<char, P>& open,
        const std::array<char, P>& close
    ) noexcept -> std::size_t
    {
        return detail_parsers::find_not_in_parentheses_impl(
            text,
            val,
            err,
            close,  // swapped
            open,
            detail_parsers::RFindParenthesesSearcher()
        );
    }

    template <std::size_t P>
    [[nodiscard]] auto rfind_not_in_parentheses(
        std::string_view text,
        std::string_view val,
        const std::array<char, P>& open,
        const std::array<char, P>& close
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
}
#endif
