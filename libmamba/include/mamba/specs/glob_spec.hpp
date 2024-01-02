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

        GlobSpec() = default;
        explicit GlobSpec(std::string pattern);

        [[nodiscard]] auto contains(std::string_view str) const -> bool;

        /**
         * Return true if the spec will match true on any input.
         */
        [[nodiscard]] auto is_free() const -> bool;

        /**
         * Return true if the spec will match exaclty one input.
         */
        [[nodiscard]] auto is_exact() const -> bool;

        [[nodiscard]] auto str() const -> const std::string&;

    private:

        std::string m_pattern = std::string(free_pattern);
    };
}

template <>
struct fmt::formatter<mamba::specs::GlobSpec>
{
    auto parse(format_parse_context& ctx) -> decltype(ctx.begin());

    auto format(const ::mamba::specs::GlobSpec& spec, format_context& ctx) -> decltype(ctx.out());
};

#endif
