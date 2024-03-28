// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <cassert>

#include <fmt/format.h>

#include "mamba/specs/regex_spec.hpp"
#include "mamba/util/string.hpp"

namespace mamba::specs
{
    namespace
    {
        template <typename Range, typename T>
        [[nodiscard]] auto rng_contains(const Range& rng, const T& elem) -> bool
        {
            return std::find(rng.begin(), rng.end(), elem) != rng.end();
        }
    }

    auto RegexSpec::parse(std::string pattern) -> expected_parse_t<RegexSpec>
    {
        // No other mean of getting parse result with ``std::regex``, but parse error need
        // to be handled by ``tl::expected`` to be managed down the road.
        try
        {
            auto regex = std::regex(pattern);
            return { { std::move(regex), std::move(pattern) } };
        }
        catch (const std::regex_error& e)
        {
            return make_unexpected_parse(e.what());
        }
    }

    RegexSpec::RegexSpec()
        : RegexSpec(std::regex(free_pattern.data(), free_pattern.size()), std::string(free_pattern))
    {
    }

    RegexSpec::RegexSpec(std::regex pattern, std::string raw_pattern)
        : m_pattern(std::move(pattern))
        , m_raw_pattern(std::move(raw_pattern))
    {
        // We force regex to start with `^` and end with `$` to simplify the multiple
        // possible representations, and because this is the safest way we can make sure it is
        // not a glob when serializing it.
        if (!util::starts_with(m_raw_pattern, pattern_start))
        {
            m_raw_pattern.insert(m_raw_pattern.begin(), pattern_start);
        }
        if (!util::ends_with(m_raw_pattern, pattern_end))
        {
            m_raw_pattern.push_back(pattern_end);
        }
    }

    auto RegexSpec::contains(std::string_view str) const -> bool
    {
        return std::regex_match(str.cbegin(), str.cend(), m_pattern);
    }

    auto RegexSpec::is_explicitly_free() const -> bool
    {
        assert(util::starts_with(m_raw_pattern, pattern_start));
        assert(util::ends_with(m_raw_pattern, pattern_end));
        return std::string_view(m_raw_pattern).substr(1, m_raw_pattern.size() - 2) == free_pattern;
    }

    auto RegexSpec::is_exact() const -> bool
    {
        constexpr auto no_special_meaning = [](char c)
        { return util::is_alphanum(c) || (c == '-') || (c == '_'); };
        assert(util::starts_with(m_raw_pattern, pattern_start));
        assert(util::ends_with(m_raw_pattern, pattern_end));
        return std::all_of(m_raw_pattern.cbegin() + 1, m_raw_pattern.cend() - 1, no_special_meaning);
    }

    auto RegexSpec::str() const -> const std::string&
    {
        return m_raw_pattern;
    }
}

auto
fmt::formatter<mamba::specs::RegexSpec>::parse(format_parse_context& ctx) -> decltype(ctx.begin())
{
    // make sure that range is empty
    if (ctx.begin() != ctx.end() && *ctx.begin() != '}')
    {
        throw fmt::format_error("Invalid format");
    }
    return ctx.begin();
}

auto
fmt::formatter<mamba::specs::RegexSpec>::format(
    const ::mamba::specs::RegexSpec& spec,
    format_context& ctx
) -> decltype(ctx.out())
{
    return fmt::format_to(ctx.out(), "{}", spec.str());
}
