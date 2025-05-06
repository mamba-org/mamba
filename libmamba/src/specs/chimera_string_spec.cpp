// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <cassert>
#include <type_traits>

#include <fmt/format.h>

#include "mamba/specs/chimera_string_spec.hpp"
#include "mamba/specs/regex_spec.hpp"
#include "mamba/util/string.hpp"

namespace mamba::specs
{
    namespace
    {
        [[nodiscard]] auto is_likely_regex(std::string_view pattern) -> bool
        {
            return util::starts_with(pattern, RegexSpec::pattern_start)
                   || util::ends_with(pattern, RegexSpec::pattern_end);
        }

        [[nodiscard]] auto make_regex(std::string pattern) -> expected_parse_t<ChimeraStringSpec>
        {
            return RegexSpec::parse(std::move(pattern))
                .transform(
                    [](RegexSpec&& spec) -> ChimeraStringSpec
                    {
                        // Chose a lighter implementation when possible
                        if (spec.is_explicitly_free())
                        {
                            return {};
                        }
                        if (spec.is_exact())
                        {
                            return ChimeraStringSpec{ GlobSpec(std::move(spec).to_string()) };
                        }
                        return ChimeraStringSpec{ std::move(spec) };
                    }
                );
        }

        [[nodiscard]] auto is_likely_glob(std::string_view pattern) -> bool
        {
            constexpr auto is_likely_glob_char = [](char c) -> bool
            {
                return util::is_alphanum(c) || (c == '-') || (c == '_')
                       || (c == GlobSpec::glob_pattern);
            };

            return pattern.empty() || (pattern == GlobSpec::free_pattern)
                   || std::all_of(pattern.cbegin(), pattern.cend(), is_likely_glob_char);
        }
    }

    auto ChimeraStringSpec::parse(std::string pattern) -> expected_parse_t<ChimeraStringSpec>
    {
        if (is_likely_regex(pattern))
        {
            return make_regex(std::move(pattern));
        }
        if (is_likely_glob(pattern))
        {
            return { ChimeraStringSpec(GlobSpec(std::move(pattern))) };
        }
        return make_regex(pattern).or_else(
            [&](const auto& /* error */) -> expected_parse_t<ChimeraStringSpec>
            { return { ChimeraStringSpec(GlobSpec(std::move(pattern))) }; }
        );
    }

    ChimeraStringSpec::ChimeraStringSpec()
        : ChimeraStringSpec(GlobSpec())
    {
    }

    ChimeraStringSpec::ChimeraStringSpec(Chimera spec)
        : m_spec(std::move(spec))
    {
    }

    auto ChimeraStringSpec::contains(std::string_view str) const -> bool
    {
        return std::visit([&](const auto& s) { return s.contains(str); }, m_spec);
    }

    auto ChimeraStringSpec::is_explicitly_free() const -> bool
    {
        return std::visit(
            [](const auto& s) -> bool
            {
                using S = std::decay_t<decltype(s)>;
                if constexpr (std::is_same_v<S, GlobSpec>)
                {
                    return s.is_free();
                }
                else if constexpr (std::is_same_v<S, RegexSpec>)
                {
                    return s.is_explicitly_free();
                }
                // All variant cases are not handled.
                assert(false);
            },
            m_spec
        );
    }

    auto ChimeraStringSpec::is_exact() const -> bool
    {
        return std::visit([](const auto& s) { return s.is_exact(); }, m_spec);
    }

    auto ChimeraStringSpec::is_glob() const -> bool
    {
        return std::holds_alternative<GlobSpec>(m_spec);
    }

    auto ChimeraStringSpec::to_string() const -> const std::string&
    {
        return std::visit([](const auto& s) -> decltype(auto) { return s.to_string(); }, m_spec);
    }
}

auto
fmt::formatter<mamba::specs::ChimeraStringSpec>::parse(format_parse_context& ctx)
    -> decltype(ctx.begin())
{
    // make sure that range is empty
    if (ctx.begin() != ctx.end() && *ctx.begin() != '}')
    {
        throw fmt::format_error("Invalid format");
    }
    return ctx.begin();
}

auto
fmt::formatter<mamba::specs::ChimeraStringSpec>::format(
    const ::mamba::specs::ChimeraStringSpec& spec,
    format_context& ctx
) const -> decltype(ctx.out())
{
    return fmt::format_to(ctx.out(), "{}", spec.to_string());
}
