// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fmt/format.h>

#include "mamba/specs/glob_spec.hpp"
#include "mamba/util/parsers.hpp"
#include "mamba/util/string.hpp"

namespace mamba::specs
{
    GlobSpec::GlobSpec(std::string pattern)
        : m_pattern(std::move(pattern))
    {
        // Not sure what to make of empty patterns... A "match nothing"?
        if (m_pattern.empty())
        {
            m_pattern = free_pattern;
        }
    }

    auto GlobSpec::contains(std::string_view str) const -> bool
    {
        // is_free is not required but returns faster in the default case.
        return is_free() || util::glob_match(m_pattern, str, glob_pattern);
    }

    auto GlobSpec::is_free() const -> bool
    {
        return m_pattern == free_pattern;
    }

    auto GlobSpec::is_exact() const -> bool
    {
        return !util::contains(m_pattern, glob_pattern);
    }

    auto GlobSpec::to_string() const -> const std::string&
    {
        return m_pattern;
    }
}

auto
fmt::formatter<mamba::specs::GlobSpec>::parse(format_parse_context& ctx) -> decltype(ctx.begin())
{
    // make sure that range is empty
    if (ctx.begin() != ctx.end() && *ctx.begin() != '}')
    {
        throw fmt::format_error("Invalid format");
    }
    return ctx.begin();
}

auto
fmt::formatter<mamba::specs::GlobSpec>::format(const ::mamba::specs::GlobSpec& spec, format_context& ctx) const
    -> decltype(ctx.out())
{
    return fmt::format_to(ctx.out(), "{}", spec.to_string());
}
