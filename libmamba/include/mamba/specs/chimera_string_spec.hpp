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

        [[nodiscard]] auto str() const -> const std::string&;

    private:

        Chimera m_spec;
    };
}

template <>
struct fmt::formatter<mamba::specs::ChimeraStringSpec>
{
    auto parse(format_parse_context& ctx) -> decltype(ctx.begin());

    auto
    format(const ::mamba::specs::ChimeraStringSpec& spec, format_context& ctx) -> decltype(ctx.out());
};

#endif
