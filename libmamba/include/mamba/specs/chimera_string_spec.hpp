// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_CHIMERA_STRING_SPEC
#define MAMBA_SPECS_CHIMERA_STRING_SPEC

#include <string_view>
#include <variant>

#include <fmt/core.h>
#include <fmt/format.h>

#include "mamba/specs/error.hpp"
#include "mamba/specs/glob_spec.hpp"
#include "mamba/specs/regex_spec.hpp"

namespace mamba::specs
{
    /**
     * A matcher for either a glob or a regex expression.
     */
    class ChimeraStringSpec
    {
    public:

        using Chimera = std::variant<GlobSpec, RegexSpec>;

        [[nodiscard]] static auto parse(std::string pattern) -> expected_parse_t<ChimeraStringSpec>;

        ChimeraStringSpec();
        explicit ChimeraStringSpec(Chimera spec);

        [[nodiscard]] auto contains(std::string_view str) const -> bool;

        /**
         * Return true if the spec will match true on any input.
         */
        [[nodiscard]] auto is_explicitly_free() const -> bool;

        /**
         * Return true if the spec will match exactly one input.
         */
        [[nodiscard]] auto is_exact() const -> bool;

        /**
         * Return true if the spec is a glob and not a regex.
         */
        [[nodiscard]] auto is_glob() const -> bool;

        [[nodiscard]] auto to_string() const -> const std::string&;

        // TODO(C++20): replace by the `= default` implementation of `operator==`
        [[nodiscard]] auto operator==(const ChimeraStringSpec& other) const -> bool
        {
            return m_spec == other.m_spec;
        }

        [[nodiscard]] auto operator!=(const ChimeraStringSpec& other) const -> bool
        {
            return !(*this == other);
        }

    private:

        Chimera m_spec;
    };
}

template <>
struct fmt::formatter<mamba::specs::ChimeraStringSpec>
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

    auto format(const ::mamba::specs::ChimeraStringSpec& spec, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct std::hash<mamba::specs::ChimeraStringSpec>
{
    auto operator()(const mamba::specs::ChimeraStringSpec& spec) const -> std::size_t;
};

#endif
