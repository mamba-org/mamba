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

    auto find_not_in_parentheses(  //
        std::string_view text,
        std::string_view val,
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
}
#endif
