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
#include <fmt/format.h>

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
        explicit RegexSpec(std::string raw_pattern);

        [[nodiscard]] auto contains(std::string_view str) const -> bool;

        /**
         * Return true if the spec will match true on any input.
         */
        [[nodiscard]] auto is_explicitly_free() const -> bool;

        /**
         * Return true if the spec will match exactly one input.
         */
        [[nodiscard]] auto is_exact() const -> bool;

        [[nodiscard]] auto to_string() const -> const std::string&;

        // TODO(C++20): replace by the `= default` implementation of `operator==`
        [[nodiscard]] auto operator==(const RegexSpec& other) const -> bool
        {
            return m_raw_pattern == other.m_raw_pattern
                   && m_pattern.flags() == other.m_pattern.flags();
        }

        [[nodiscard]] auto operator!=(const RegexSpec& other) const -> bool
        {
            return !(*this == other);
        }

    private:

        std::string m_raw_pattern;
        std::regex m_pattern;
    };
}

template <>
struct fmt::formatter<mamba::specs::RegexSpec>
{
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        // make sure that range is empty
        if (ctx.begin() != ctx.end() && *ctx.begin() != '}')
        {
            throw fmt::format_error("Invalid format");
        }
        return ctx.begin();
    }

    auto format(const ::mamba::specs::RegexSpec& spec, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct std::hash<mamba::specs::RegexSpec>
{
    auto operator()(const mamba::specs::RegexSpec& spec) const -> std::size_t;
};

#endif
