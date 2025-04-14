// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <array>
#include <type_traits>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "mamba/specs/version_spec.hpp"
#include "mamba/util/string.hpp"

#include "specs/version_spec_impl.hpp"

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
    VersionPredicate::not_starts_with::operator()(const Version& point, const Version& prefix) const
        -> bool
    {
        return !point.starts_with(prefix);
    }

    auto operator==(VersionPredicate::not_starts_with, VersionPredicate::not_starts_with) -> bool
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

    namespace
    {
        auto version_match_glob(const CommonVersion& candidate, const CommonVersion& pattern) -> bool
        {
            auto cand_it = candidate.cbegin();
            const auto cand_last = candidate.cend();
            auto pat_it = pattern.cbegin();
            const auto pat_last = pattern.cend();
            auto parts_required = std::size_t(0);
            auto parts_available = std::size_t(0);

            constexpr auto is_failed = [](auto req, auto avail) -> bool
            { return (req > avail) || (req == 0 && avail > 0); };
            constexpr auto is_glob_part = [](const auto& p) -> bool
            { return p == VERSION_GLOB_SEGMENT; };
            constexpr auto distance = [](auto i1, auto i2)
            {
                assert(i1 <= i2);
                return static_cast<std::size_t>(std::distance(i1, i2));
            };

            while (pat_it != pat_last)
            {
                // We move forward in the pattern counting all the contiguous glob parts.
                const auto pat_sub_first = std::find_if_not(pat_it, pat_last, is_glob_part);
                parts_required += distance(pat_it, pat_sub_first);
                pat_it = pat_sub_first;

                // No more explicit subpatterns (i.e. not globs) to match
                if (pat_it == pat_last)
                {
                    break;
                }

                // Find the end of the sub pattern, i.e. to the start of the next glob.
                // This is required to avoid greedily matching on the first similar character.
                const auto pat_sub_last = std::find_if(pat_sub_first, pat_last, is_glob_part);

                // At this point we have a required pattern.
                // We search for it in the given version and count the parts that were skipped.
                const auto cand_sub_first = std::search(cand_it, cand_last, pat_sub_first, pat_sub_last);
                parts_available += distance(cand_it, cand_sub_first);
                cand_it = cand_sub_first;

                // If we exhause the candidate without finding a match for the pattern it's a
                // failure
                if (cand_it == cand_last)
                {
                    return false;
                }

                // At this point we have a match.
                // We compare the number of globs found with the number of unmatched candidate parts
                if (is_failed(parts_required, parts_available))
                {
                    return false;
                }

                // We pass through the match and reset the counts
                const auto subpat_len = std::distance(pat_sub_first, pat_sub_last);
                pat_it += subpat_len;
                cand_it += subpat_len;
                parts_required = 0;
                parts_available = 0;
            }

            parts_available += static_cast<std::size_t>(std::distance(cand_it, cand_last));

            return !is_failed(parts_required, parts_available);
        }

    }

    auto VersionPredicate::version_glob::operator()(const Version& point, const Version& pattern) const
        -> bool
    {
        return (point.epoch() == pattern.epoch())
               && version_match_glob(point.version(), pattern.version())
               && version_match_glob(point.local(), pattern.local());
    }

    auto operator==(VersionPredicate::version_glob, VersionPredicate::version_glob) -> bool
    {
        return true;
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

    auto VersionPredicate::make_not_starts_with(Version ver) -> VersionPredicate
    {
        return VersionPredicate(std::move(ver), not_starts_with{});
    }

    auto VersionPredicate::make_compatible_with(Version ver, std::size_t level) -> VersionPredicate
    {
        return VersionPredicate(std::move(ver), compatible_with{ level });
    }

    auto VersionPredicate::make_version_glob(Version pattern) -> VersionPredicate
    {
        return VersionPredicate(std::move(pattern), version_glob{});
    }

    auto VersionPredicate::str() const -> std::string
    {
        return fmt::format("{}", *this);
    }

    auto VersionPredicate::str_conda_build() const -> std::string
    {
        return fmt::format("{:b}", *this);
    }

    VersionPredicate::VersionPredicate(Version ver, BinaryOperator op)
        : m_version(std::move(ver))
        , m_operator(std::move(op))
    {
    }

    auto operator==(const VersionPredicate& lhs, const VersionPredicate& rhs) -> bool
    {
        return (lhs.m_operator == rhs.m_operator)  //
               && (lhs.m_version == rhs.m_version)
               // In version_glob, the version is not understood purely as a version since ``*``
               // has different meaning, as explicit trailing zeros.
               // Versions should be made part of this variant internal and handled there.
               && (!std::holds_alternative<VersionPredicate::version_glob>(lhs.m_operator)
                   || lhs.m_version.version().size() == rhs.m_version.version().size());
    }

    auto operator!=(const VersionPredicate& lhs, const VersionPredicate& rhs) -> bool
    {
        return !(lhs == rhs);
    }
}

auto
fmt::formatter<mamba::specs::VersionPredicate>::parse(format_parse_context& ctx)
    -> decltype(ctx.begin())
{
    if (auto it = std::find(ctx.begin(), ctx.end(), 'b'); it < ctx.end())
    {
        conda_build_form = true;
        return ++it;
    }
    return ctx.begin();
}

auto
fmt::formatter<mamba::specs::VersionPredicate>::format(
    const ::mamba::specs::VersionPredicate& pred,
    format_context& ctx
) const -> decltype(ctx.out())
{
    using VersionPredicate = typename mamba::specs::VersionPredicate;
    using VersionSpec = typename mamba::specs::VersionSpec;
    using Version = typename mamba::specs::Version;

    auto out = ctx.out();
    std::visit(
        [&](const auto& op)
        {
            using Op = std::decay_t<decltype(op)>;
            if constexpr (std::is_same_v<Op, VersionPredicate::free_interval>)
            {
                out = fmt::format_to(
                    out,
                    "{}{}",
                    VersionSpec::starts_with_str,
                    VersionSpec::glob_pattern_str
                );
            }
            if constexpr (std::is_same_v<Op, std::equal_to<Version>>)
            {
                out = fmt::format_to(out, "{}{}", VersionSpec::equal_str, pred.m_version);
            }
            if constexpr (std::is_same_v<Op, std::not_equal_to<Version>>)
            {
                out = fmt::format_to(out, "{}{}", VersionSpec::not_equal_str, pred.m_version);
            }
            if constexpr (std::is_same_v<Op, std::greater<Version>>)
            {
                out = fmt::format_to(out, "{}{}", VersionSpec::greater_str, pred.m_version);
            }
            if constexpr (std::is_same_v<Op, std::greater_equal<Version>>)
            {
                out = fmt::format_to(out, "{}{}", VersionSpec::greater_equal_str, pred.m_version);
            }
            if constexpr (std::is_same_v<Op, std::less<Version>>)
            {
                out = fmt::format_to(out, "{}{}", VersionSpec::less_str, pred.m_version);
            }
            if constexpr (std::is_same_v<Op, std::less_equal<Version>>)
            {
                out = fmt::format_to(out, "{}{}", VersionSpec::less_equal_str, pred.m_version);
            }
            if constexpr (std::is_same_v<Op, VersionPredicate::starts_with>)
            {
                if (conda_build_form)
                {
                    out = fmt::format_to(out, "{}{}", pred.m_version, VersionSpec::glob_suffix_str);
                }
                else
                {
                    out = fmt::format_to(out, "{}{}", VersionSpec::starts_with_str, pred.m_version);
                }
            }
            if constexpr (std::is_same_v<Op, VersionPredicate::not_starts_with>)
            {
                out = fmt::format_to(
                    out,
                    "{}{}{}",
                    VersionSpec::not_equal_str,
                    pred.m_version,
                    VersionSpec::glob_suffix_str
                );
            }
            if constexpr (std::is_same_v<Op, VersionPredicate::compatible_with>)
            {
                // Make sure to print the version without loosing information.
                auto version_level = pred.m_version.version().size();
                auto format_level = std::max(op.level, version_level);
                out = fmt::format_to(
                    out,
                    "{}{}",
                    VersionSpec::compatible_str,
                    pred.m_version.str(format_level)
                );
            }
            if constexpr (std::is_same_v<Op, VersionPredicate::version_glob>)
            {
                out = fmt::format_to(out, "{:g}", pred.m_version);
            }
        },
        pred.m_operator
    );
    return out;
}

namespace mamba::specs
{

    /********************************
     *  VersionSpec Implementation  *
     ********************************/

    auto VersionSpec::from_predicate(VersionPredicate pred) -> VersionSpec
    {
        auto inner_tree = tree_type::tree_type();
        inner_tree.add_leaf(std::move(pred));
        return VersionSpec{ tree_type(std::move(inner_tree)) };
    }

    VersionSpec::VersionSpec(tree_type&& tree) noexcept
        : m_tree(std::move(tree))
    {
    }

    auto VersionSpec::contains(const Version& point) const -> bool
    {
        return m_tree.evaluate([&point](const auto& node) { return node.contains(point); });
    }

    auto VersionSpec::is_explicitly_free() const -> bool
    {
        const auto free_pred = VersionPredicate::make_free();
        const auto is_free_pred = [&free_pred](const auto& node) { return node == free_pred; };
        return m_tree.empty() || ((m_tree.size() == 1) && m_tree.evaluate(is_free_pred));
    }

    auto VersionSpec::str() const -> std::string
    {
        return fmt::format("{}", *this);
    }

    auto VersionSpec::str_conda_build() const -> std::string
    {
        return fmt::format("{:b}", *this);
    }

    auto VersionSpec::expression_size() const -> std::size_t
    {
        return m_tree.size();
    }

    namespace
    {
        template <typename Val, typename Range>
        constexpr auto equal_any(const Val& val, const Range& range) -> bool
        {
            return std::find(range.cbegin(), range.cend(), val) != range.cend();
        }

        auto parse_op_and_version(std::string_view str) -> expected_parse_t<VersionPredicate>
        {
            str = util::strip(str);
            // WARNING order is important since some operator are prefix of others.
            if (str.empty() || equal_any(str, VersionSpec::all_free_strs))
            {
                return VersionPredicate::make_free();
            }
            if (util::starts_with(str, VersionSpec::greater_equal_str))
            {
                return Version::parse(util::lstrip(str.substr(VersionSpec::greater_equal_str.size())))
                    .transform([](specs::Version&& ver)
                               { return VersionPredicate::make_greater_equal(std::move(ver)); });
            }
            if (util::starts_with(str, VersionSpec::greater_str))
            {
                return Version::parse(util::lstrip(str.substr(VersionSpec::greater_str.size())))
                    .transform([](specs::Version&& ver)
                               { return VersionPredicate::make_greater(std::move(ver)); });
            }
            if (util::starts_with(str, VersionSpec::less_equal_str))
            {
                return Version::parse(util::lstrip(str.substr(VersionSpec::less_equal_str.size())))
                    .transform([](specs::Version&& ver)
                               { return VersionPredicate::make_less_equal(std::move(ver)); });
            }
            if (util::starts_with(str, VersionSpec::less_str))
            {
                return Version::parse(util::lstrip(str.substr(VersionSpec::less_str.size())))
                    .transform([](specs::Version&& ver)
                               { return VersionPredicate::make_less(std::move(ver)); });
            }
            if (util::starts_with(str, VersionSpec::compatible_str))
            {
                return Version::parse(util::lstrip(str.substr(VersionSpec::compatible_str.size())))
                    .transform(
                        [](specs::Version&& ver)
                        {
                            // in ``~=1.1`` level is assumed to be 1, in ``~=1.1.1`` level 2, etc.
                            static constexpr auto one = std::size_t(1);  // MSVC
                            const std::size_t level = std::max(ver.version().size(), one) - one;
                            return VersionPredicate::make_compatible_with(std::move(ver), level);
                        }
                    );
            }
            const bool has_glob_suffix = util::ends_with(str, VersionSpec::glob_suffix_str);
            const std::size_t glob_len = has_glob_suffix * VersionSpec::glob_suffix_str.size();
            if (util::starts_with(str, VersionSpec::equal_str))
            {
                const std::size_t start = VersionSpec::equal_str.size();
                // Glob suffix changes meaning for ==1.3.*
                if (has_glob_suffix)
                {
                    return Version::parse(
                               util::lstrip(str.substr(start, str.size() - glob_len - start))
                    )
                        .transform([](specs::Version&& ver)
                                   { return VersionPredicate::make_starts_with(std::move(ver)); });
                }
                else
                {
                    return Version::parse(util::lstrip(str.substr(start)))
                        .transform([](specs::Version&& ver)
                                   { return VersionPredicate::make_equal_to(std::move(ver)); });
                }
            }
            if (util::starts_with(str, VersionSpec::not_equal_str))
            {
                const std::size_t start = VersionSpec::not_equal_str.size();
                // Glob suffix changes meaning for !=1.3.*
                if (has_glob_suffix)
                {
                    return Version::parse(
                               util::lstrip(str.substr(start, str.size() - glob_len - start))
                    )
                        .transform([](specs::Version&& ver)
                                   { return VersionPredicate::make_not_starts_with(std::move(ver)); }
                        );
                }
                else
                {
                    return Version::parse(util::lstrip(str.substr(start)))
                        .transform([](specs::Version&& ver)
                                   { return VersionPredicate::make_not_equal_to(std::move(ver)); });
                }
            }
            if (util::starts_with(str, VersionSpec::starts_with_str))
            {
                const std::size_t start = VersionSpec::starts_with_str.size();
                // Glob suffix does not change meaning for =1.3.*
                return Version::parse(util::lstrip(str.substr(start, str.size() - glob_len - start)))
                    .transform([](specs::Version&& ver)
                               { return VersionPredicate::make_starts_with(std::move(ver)); });
            }
            // At this point, globs that could be handled in a simpler way (free "*",
            // starts with "=1.*") are already processed.
            // If we find a glob suffix in the end only, we leave it to be processed as a starts
            // with in the next block.
            // The check for whether the "*" is a glob or a valid version character is poorly
            // defined.
            const bool is_glob = util::starts_with(str, VersionSpec::glob_pattern_str)
                                 || (str.find(VersionSpec::glob_suffix_str)
                                     < std::max(str.size(), std::size_t(2)) - 2);
            if (is_glob)
            {
                return Version::parse(util::lstrip(str))
                    .transform([](specs::Version&& ver)
                               { return VersionPredicate::make_version_glob(std::move(ver)); });
            }
            // All versions must start with either a digit or a lowercase letter
            // The version regex should comply with r"^[\*\.\+!_0-9a-z]+$"
            // cf. https://github.com/conda/conda/blob/main/conda/models/version.py#L33
            // Note that we don't apply this condition when the version is given with an operator
            // In that case, string literals are converted to lowercase in `version.cpp` through
            // `Version::parse`
            if (util::is_digit(str.front()) || util::is_lower(str.front()))
            {
                // Glob suffix does  change meaning for 1.3.* and 1.3*
                if (util::ends_with(str, VersionSpec::glob_suffix_str.back()))
                {
                    // either ".*" or "*"
                    static constexpr auto one = std::size_t(1);  // MSVC
                    const std::size_t len = str.size() - std::max(glob_len, one);
                    return Version::parse(util::lstrip(str.substr(0, len)))
                        .transform([](specs::Version&& ver)
                                   { return VersionPredicate::make_starts_with(std::move(ver)); });
                }
                else
                {
                    return Version::parse(util::lstrip(str))
                        .transform([](specs::Version&& ver)
                                   { return VersionPredicate::make_equal_to(std::move(ver)); });
                }
            }
            return tl::make_unexpected(
                ParseError(fmt::format(R"(Found invalid version predicate in "{}")", str))
            );
        }
    }

    auto VersionSpec::parse(std::string_view str) -> expected_parse_t<VersionSpec>
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
        str = util::lstrip(str);

        // Explicit short-circuiting for "free" spec
        // This case would be handled anyway but we can avoid allocating a tree in this
        // likely case.
        if (str.empty() || equal_any(str, VersionSpec::all_free_strs))
        {
            return {};
        }

        while (!str.empty())
        {
            if (str.front() == VersionSpec::and_token)
            {
                const bool pushed = parser.push_operator(util::BoolOperator::logical_and);
                if (!pushed)
                {
                    return tl::make_unexpected(ParseError(fmt::format(
                        R"(Found unexpected token "{}" in version spec "{}")",
                        VersionSpec::and_token,
                        str
                    )));
                }
                str = util::lstrip(str.substr(1));
            }
            else if (str.front() == VersionSpec::or_token)
            {
                const bool pushed = parser.push_operator(util::BoolOperator::logical_or);
                if (!pushed)
                {
                    return tl::make_unexpected(ParseError(fmt::format(
                        R"(Found unexpected token "{}" in version spec "{}")",
                        VersionSpec::or_token,
                        str
                    )));
                }
                str = util::lstrip(str.substr(1));
            }
            else if (str.front() == VersionSpec::left_parenthesis_token)
            {
                const bool pushed = parser.push_left_parenthesis();
                if (!pushed)
                {
                    return tl::make_unexpected(ParseError(fmt::format(
                        R"(Found unexpected token "{}" in version spec "{}")",
                        VersionSpec::left_parenthesis_token,
                        str
                    )));
                }
                str = util::lstrip(str.substr(1));
            }
            else if (str.front() == VersionSpec::right_parenthesis_token)
            {
                const bool pushed = parser.push_right_parenthesis();
                if (!pushed)
                {
                    return tl::make_unexpected(ParseError(fmt::format(
                        R"(Found unexpected token "{}" in version spec "{}")",
                        VersionSpec::right_parenthesis_token,
                        str
                    )));
                }
                str = util::lstrip(str.substr(1));
            }
            else
            {
                auto [op_ver, rest] = util::lstrip_if_parts(str, [&](auto c) { return !is_token(c); });
                auto pred = parse_op_and_version(op_ver);
                if (!pred.has_value())
                {
                    return tl::make_unexpected(pred.error());
                }
                const bool pushed = parser.push_variable(std::move(pred).value());
                if (!pushed)
                {
                    return tl::make_unexpected(ParseError(fmt::format(
                        R"(Found unexpected version predicate "{}" in version spec "{}")",
                        pred.value(),
                        str
                    )));
                }
                str = util::lstrip(rest);
            }
        }

        const bool correct = parser.finalize();
        if (!correct)
        {
            return tl::make_unexpected(
                ParseError(fmt::format(R"(Version spec "{}" is an incorrect expression)", str))
            );
        }

        return { VersionSpec{ std::move(parser).tree() } };
    }

    namespace version_spec_literals
    {
        auto operator""_vs(const char* str, std::size_t len) -> VersionSpec
        {
            return VersionSpec::parse(std::literals::string_view_literals::operator""sv(str, len))
                .or_else([](ParseError&& error) { throw std::move(error); })
                .value();
        }
    }
}

auto
fmt::formatter<mamba::specs::VersionSpec>::parse(format_parse_context& ctx) -> decltype(ctx.begin())
{
    if (auto it = std::find(ctx.begin(), ctx.end(), 'b'); it < ctx.end())
    {
        conda_build_form = true;
        return ++it;
    }
    return ctx.begin();
}

auto
fmt::formatter<mamba::specs::VersionSpec>::format(
    const ::mamba::specs::VersionSpec& spec,
    format_context& ctx
) const -> decltype(ctx.out())
{
    using VersionSpec = typename mamba::specs::VersionSpec;

    auto out = ctx.out();
    if (spec.m_tree.empty())
    {
        return fmt::format_to(out, "{}", VersionSpec::preferred_free_str);
    }
    spec.m_tree.infix_for_each(
        [&](const auto& token)
        {
            using tree_type = typename VersionSpec::tree_type;
            using Token = std::decay_t<decltype(token)>;
            if constexpr (std::is_same_v<Token, tree_type::LeftParenthesis>)
            {
                out = fmt::format_to(out, "{}", VersionSpec::left_parenthesis_token);
            }
            if constexpr (std::is_same_v<Token, tree_type::RightParenthesis>)
            {
                out = fmt::format_to(out, "{}", VersionSpec::right_parenthesis_token);
            }
            if constexpr (std::is_same_v<Token, tree_type::operator_type>)
            {
                if (token == tree_type::operator_type::logical_or)
                {
                    out = fmt::format_to(out, "{}", VersionSpec::or_token);
                }
                else
                {
                    out = fmt::format_to(out, "{}", VersionSpec::and_token);
                }
            }
            if constexpr (std::is_same_v<Token, tree_type::variable_type>)
            {
                out = fmt::format_to(out, conda_build_form ? "{:b}" : "{}", token);
            }
        }
    );
    return out;
}
