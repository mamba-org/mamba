// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_GLOB_SPEC
#define MAMBA_SPECS_GLOB_SPEC

#include <string>
#include <string_view>

#include <fmt/core.h>
#include <fmt/format.h>

namespace mamba::specs
{
    /**
     * A matcher for glob expression.
     *
     * Currently only support "*" wildcard for matching zero or more characters.
     */
    class GlobSpec
    {
    public:

        inline static constexpr std::string_view free_pattern = "*";
        inline static constexpr char glob_pattern = '*';

        GlobSpec() = default;
        explicit GlobSpec(std::string pattern);

        [[nodiscard]] auto contains(std::string_view str) const -> bool;

        /**
         * Return true if the spec will match true on any input.
         */
        [[nodiscard]] auto is_free() const -> bool;

        /**
         * Return true if the spec will match exactly one input.
         */
        [[nodiscard]] auto is_exact() const -> bool;

        [[nodiscard]] auto to_string() const -> const std::string&;

        // TODO(C++20): replace by the `= default` implementation of `operator==`
        [[nodiscard]] auto operator==(const GlobSpec& other) const -> bool
        {
            return m_pattern == other.m_pattern;
        }

        [[nodiscard]] auto operator!=(const GlobSpec& other) const -> bool
        {
            return !(*this == other);
        }

    private:

        std::string m_pattern = std::string(free_pattern);
    };
}

template <>
struct fmt::formatter<mamba::specs::GlobSpec>
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

    auto format(const ::mamba::specs::GlobSpec& spec, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct std::hash<mamba::specs::GlobSpec>
{
    auto operator()(const mamba::specs::GlobSpec& spec) const -> std::size_t;
};

#endif
