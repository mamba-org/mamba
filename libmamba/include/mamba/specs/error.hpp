// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_ERROR_HPP
#define MAMBA_SPECS_ERROR_HPP

#include <stdexcept>
#include <utility>

#include <tl/expected.hpp>

namespace mamba::specs
{

    struct ParseError final : std::invalid_argument
    {
        using std::invalid_argument::invalid_argument;
    };

    template <typename T>
    using expected_parse_t = tl::expected<T, ParseError>;

    template <typename T>
    [[nodiscard]] auto make_unexpected_parse(T&& err) -> tl::unexpected<ParseError>;

    /********************
     *  Implementation  *
     ********************/

    template <typename T>
    auto make_unexpected_parse(T&& err) -> tl::unexpected<ParseError>
    {
        return tl::make_unexpected(ParseError(std::forward<T>(err)));
    }
}
#endif
