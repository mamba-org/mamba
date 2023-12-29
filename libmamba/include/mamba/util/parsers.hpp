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
     * Correctly matches parenteses together so that inner parenthesese pairs are skipped.
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
     * Correctly matches parenteses together so that inner parenthesese pairs are skipped.
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
}
#endif
