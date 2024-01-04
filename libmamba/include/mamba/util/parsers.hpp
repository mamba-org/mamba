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

namespace mamba::util
{

    enum struct ParseError
    {
        Ok,
        InvalidInput,
        NotFound,
    };

    /**
     * Find the next matching parenthesese pair.
     *
     * Correctly matches parenteses together so that inner parentheses pairs are skipped.
     * Open and closing pairs need not be differents.
     * If an error is encountered, @p err is modified to contain the error, otherwise it is left
     * as it is.
     */
    auto find_matching_parentheses_idx(  //
        std::string_view text,
        ParseError& err,
        char open = '(',
        char close = ')'
    ) noexcept -> std::pair<std::size_t, std::size_t>;

    /**
     * Find the next matching parenthesese pair.
     *
     * Correctly matches parentheses together so that inner parentheses pairs are skipped.
     * Open and closing pairs need not be differents.
     */
    [[nodiscard]] auto find_matching_parentheses_idx(  //
        std::string_view text,
        char open = '(',
        char close = ')'
    ) noexcept -> tl::expected<std::pair<std::size_t, std::size_t>, ParseError>;

    auto find_matching_parentheses_str(  //
        std::string_view text,
        ParseError& err,
        char open = '(',
        char close = ')'
    ) noexcept -> std::string_view;

    [[nodiscard]] auto find_matching_parentheses_str(  //
        std::string_view text,
        char open = '(',
        char close = ')'
    ) noexcept -> tl::expected<std::string_view, ParseError>;

    /**
     * Find a character, except in mathcing parentheses pairs.
     *
     * Find the first occurence of the given character, except if such character is inside a valid
     * pair of parentheses.
     * Open and closing pairs need not be differents.
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

    /**
     * Test wether the glob pattern @p pattern matches the string @p str.
     */
    [[nodiscard]] auto glob_match(std::string_view pattern, std::string_view str, char glob = '*')
        -> bool;

    /********************
     *  Implementation  *
     ********************/

    namespace detail
    {
        template <typename T, typename... Arr>
        constexpr auto concat_array(const Arr&... arrs)
        {
            auto out = std::array<T, (... + sizeof(arrs))>{};
            std::size_t out_idx = 0;
            auto copy_one = [&](const auto& a)
            {
                for (auto const& x : a)
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
            auto pos = std::size_t(N);
            for (std::size_t i = 0; i < N; ++i)
            {
                const bool found = arr[i] == val;
                pos = static_cast<int>(found) * i + (1 - static_cast<int>(found)) * pos;
            }
            return pos;
        }
    }

    template <std::size_t P>
    auto find_not_in_parentheses(
        std::string_view text,
        char c,
        ParseError& err,
        const std::array<char, P>& open,
        const std::array<char, P>& close
    ) noexcept -> std::size_t
    {
        // TODO(C++20): After allocating tokens and depths here, call an impl function using
        // std::span defined in .cpp
        static constexpr auto npos = std::string_view::npos;

        const auto tokens = detail::concat_array<char>(std::array{ c }, open, close);
        const auto tokens_str = std::string_view(tokens.data(), tokens.size());

        auto depths = std::array<int, P + 1>{};  // last for easy branchless access
        auto first_val_pos = npos;
        auto pos = text.find_first_of(tokens_str);
        while (pos != npos)
        {
            const auto open_pos = detail::find(open, text[pos]);
            const auto close_pos = detail::find(close, text[pos]);
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
            first_val_pos = if_else((text[pos] == c) && (pos == npos), pos, first_val_pos);
            if ((text[pos] == c) && (depths == decltype(depths){}))
            {
                return pos;
            }
            pos = text.find_first_of(tokens_str, pos + 1);
        }
        err = if_else(
            err == ParseError::Ok,
            if_else(depths == decltype(depths){}, ParseError::NotFound, ParseError::InvalidInput),
            err
        );
        return first_val_pos;
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
}
#endif
