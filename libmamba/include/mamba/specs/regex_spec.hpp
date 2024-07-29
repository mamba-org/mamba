// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_REGEX_SPEC
#define MAMBA_SPECS_REGEX_SPEC

#include <regex>
#include <string>
#include <string_view>

#include <fmt/core.h>

#include "mamba/specs/error.hpp"

namespace mamba::specs
{
    /**
     * A matcher for regex expression.
     */
    class RegexSpec
    {
    public:

        inline static constexpr std::string_view free_pattern = ".*";
        inline static constexpr char pattern_start = '^';
        inline static constexpr char pattern_end = '$';

        [[nodiscard]] static auto parse(std::string pattern) -> expected_parse_t<RegexSpec>;

        RegexSpec();
        RegexSpec(std::regex pattern, std::string raw_pattern);

        [[nodiscard]] auto contains(std::string_view str) const -> bool;

        /**
         * Return true if the spec will match true on any input.
         */
        [[nodiscard]] auto is_explicitly_free() const -> bool;

        /**
         * Return true if the spec will match exactly one input.
         */
        [[nodiscard]] auto is_exact() const -> bool;

        [[nodiscard]] auto str() const -> const std::string&;

    private:

        std::regex m_pattern;
        std::string m_raw_pattern;
    };
}

template <>
struct fmt::formatter<mamba::specs::RegexSpec>
{
    auto parse(format_parse_context& ctx) -> decltype(ctx.begin());

    auto format(const ::mamba::specs::RegexSpec& spec, format_context& ctx) -> decltype(ctx.out());
};

#endif
