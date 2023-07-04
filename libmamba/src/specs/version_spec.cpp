// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <cassert>
#include <stdexcept>
#include <type_traits>

#include <fmt/format.h>

#include "mamba/core/util_string.hpp"
#include "mamba/specs/version_spec.hpp"

namespace mamba::specs
{
    /*****************************************************
     *  VersionPredicate BinaryOperators Implementation  *
     *****************************************************/

    auto VersionPredicate::free_interval::operator()(const Version&, const Version&) const -> bool
    {
        return true;
    }

    auto operator==(VersionPredicate::free_interval, VersionPredicate::free_interval) -> bool
    {
        return true;
    }

    auto VersionPredicate::starts_with::operator()(const Version& point, const Version& prefix) const
        -> bool
    {
        return point.starts_with(prefix);
    }

    auto operator==(VersionPredicate::starts_with, VersionPredicate::starts_with) -> bool
    {
        return true;
    }

    auto
    VersionPredicate::compatible_with::operator()(const Version& point, const Version& older) const
        -> bool
    {
        return point.compatible_with(older, level);
    }

    auto operator==(VersionPredicate::compatible_with lhs, VersionPredicate::compatible_with rhs)
        -> bool
    {
        return lhs.level == rhs.level;
    }

    static auto operator==(std::equal_to<Version>, std::equal_to<Version>) -> bool
    {
        return true;
    }

    static auto operator==(std::not_equal_to<Version>, std::not_equal_to<Version>) -> bool
    {
        return true;
    }

    static auto operator==(std::greater<Version>, std::greater<Version>) -> bool
    {
        return true;
    }

    static auto operator==(std::greater_equal<Version>, std::greater_equal<Version>) -> bool
    {
        return true;
    }

    static auto operator==(std::less<Version>, std::less<Version>) -> bool
    {
        return true;
    }

    static auto operator==(std::less_equal<Version>, std::less_equal<Version>) -> bool
    {
        return true;
    }

    /*************************************
     *  VersionPredicate Implementation  *
     *************************************/

    auto VersionPredicate::contains(const Version& point) const -> bool
    {
        return std::visit([&](const auto& op) { return op(point, m_version); }, m_operator);
    }

    auto VersionPredicate::make_free() -> VersionPredicate
    {
        return VersionPredicate({}, free_interval{});
    }

    auto VersionPredicate::make_equal_to(Version ver) -> VersionPredicate
    {
        return VersionPredicate(std::move(ver), std::equal_to<Version>{});
    }

    auto VersionPredicate::make_not_equal_to(Version ver) -> VersionPredicate
    {
        return VersionPredicate(std::move(ver), std::not_equal_to<Version>{});
    }

    auto VersionPredicate::make_greater(Version ver) -> VersionPredicate
    {
        return VersionPredicate(std::move(ver), std::greater<Version>{});
    }

    auto VersionPredicate::make_greater_equal(Version ver) -> VersionPredicate
    {
        return VersionPredicate(std::move(ver), std::greater_equal<Version>{});
    }

    auto VersionPredicate::make_less(Version ver) -> VersionPredicate
    {
        return VersionPredicate(std::move(ver), std::less<Version>{});
    }

    auto VersionPredicate::make_less_equal(Version ver) -> VersionPredicate
    {
        return VersionPredicate(std::move(ver), std::less_equal<Version>{});
    }

    auto VersionPredicate::make_starts_with(Version ver) -> VersionPredicate
    {
        return VersionPredicate(std::move(ver), starts_with{});
    }

    auto VersionPredicate::make_compatible_with(Version ver, std::size_t level) -> VersionPredicate
    {
        return VersionPredicate(std::move(ver), compatible_with{ level });
    }

    VersionPredicate::VersionPredicate(Version ver, BinaryOperator op)
        : m_version(std::move(ver))
        , m_operator(std::move(op))
    {
    }

    auto operator==(const VersionPredicate& lhs, const VersionPredicate& rhs) -> bool
    {
        return (lhs.m_operator == rhs.m_operator) && (lhs.m_version == rhs.m_version);
    }

    auto operator!=(const VersionPredicate& lhs, const VersionPredicate& rhs) -> bool
    {
        return !(lhs == rhs);
    }

    /********************************
     *  VersionSpec Implementation  *
     ********************************/

    VersionSpec::VersionSpec(tree_type&& tree) noexcept
        : m_tree(std::move(tree))
    {
    }

    auto VersionSpec::contains(const Version& point) const -> bool
    {
        return m_tree.evaluate([&point](const auto& node) { return node.contains(point); });
    }

    namespace
    {
        auto parse_op_and_version(std::string_view str) -> VersionPredicate
        {
            str = lstrip(str);
            // WARNING order is important since some operator are prefix of others.
            if (str.empty())
            {
                return VersionPredicate::make_free();
            }
            if (starts_with(str, VersionSpec::greater_equal_str))
            {
                return VersionPredicate::make_greater_equal(
                    Version::parse(str.substr(VersionSpec::greater_equal_str.size()))
                );
            }
            if (starts_with(str, VersionSpec::greater_str))
            {
                return VersionPredicate::make_greater(
                    Version::parse(str.substr(VersionSpec::greater_str.size()))
                );
            }
            if (starts_with(str, VersionSpec::less_equal_str))
            {
                return VersionPredicate::make_less_equal(
                    Version::parse(str.substr(VersionSpec::less_equal_str.size()))
                );
            }
            if (starts_with(str, VersionSpec::less_str))
            {
                return VersionPredicate::make_less(
                    Version::parse(str.substr(VersionSpec::less_str.size()))
                );
            }
            if (starts_with(str, VersionSpec::equal_str))
            {
                return VersionPredicate::make_equal_to(
                    Version::parse(str.substr(VersionSpec::equal_str.size()))
                );
            }
            if (starts_with(str, VersionSpec::not_equal_str))
            {
                return VersionPredicate::make_not_equal_to(
                    Version::parse(str.substr(VersionSpec::not_equal_str.size()))
                );
            }
            if (starts_with(str, VersionSpec::compatible_str))
            {
                auto ver = Version::parse(str.substr(VersionSpec::compatible_str.size()));
                const std::size_t level = ver.version().size();
                return VersionPredicate::make_compatible_with(std::move(ver), level);
            }
            const bool has_glob_suffix = ends_with(str, VersionSpec::glob_suffix_str);
            static constexpr std::size_t glob_suffix_size = VersionSpec::glob_suffix_str.size();
            if (starts_with(str, VersionSpec::starts_with_str))
            {
                // =1.3 and =1.3.* have the same meaning
                const std::size_t start = VersionSpec::starts_with_str.size();
                const std::size_t end = str.size() - glob_suffix_size * has_glob_suffix;
                return VersionPredicate::make_starts_with(Version::parse(str.substr(start, end)));
            }
            if (is_digit(str.front()))  // All versions must start with a digit
            {
                if (has_glob_suffix)
                {
                    // 1.3.* means starts_with
                    const std::size_t end = str.size() - glob_suffix_size;
                    return VersionPredicate::make_starts_with(Version::parse(str.substr(0, end)));
                }
                else
                {
                    // 1.3 means equal
                    return VersionPredicate::make_equal_to(Version::parse(str));
                }
            }
            throw std::invalid_argument(fmt::format(R"(Found invalid version predicate in "{}")", str)
            );
        }
    }

    auto VersionSpec::parse(std::string_view str) -> VersionSpec
    {
        static constexpr auto all_tokens = std::array{
            VersionSpec::and_token,
            VersionSpec::or_token,
            VersionSpec::left_parenthesis_token,
            VersionSpec::right_parenthesis_token,
        };

        auto is_token = [&](auto c)
        { return std::find(all_tokens.cbegin(), all_tokens.cend(), c) != all_tokens.cend(); };

        auto parser = util::InfixParser<VersionPredicate, util::BoolOperator>();
        str = lstrip(str);
        while (!str.empty())
        {
            if (str.front() == VersionSpec::and_token)
            {
                parser.push_operator(util::BoolOperator::logical_and);
                str = str.substr(1);
            }
            else if (str.front() == VersionSpec::or_token)
            {
                parser.push_operator(util::BoolOperator::logical_or);
                str = str.substr(1);
            }
            else if (str.front() == VersionSpec::left_parenthesis_token)
            {
                parser.push_left_parenthesis();
                str = lstrip(str.substr(1));
            }
            else if (str.front() == VersionSpec::right_parenthesis_token)
            {
                parser.push_right_parenthesis();
                str = lstrip(str.substr(1));
            }
            else
            {
                auto [op_ver, rest] = lstrip_if_parts(str, [&](auto c) { return !is_token(c); });
                parser.push_variable(parse_op_and_version(op_ver));
                str = rest;
            }
        }
        parser.finalize();
        return VersionSpec{ std::move(parser).tree() };
    }
}
