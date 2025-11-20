// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_MATCH_SPEC_CONDITION_HPP
#define MAMBA_SPECS_MATCH_SPEC_CONDITION_HPP

#include <memory>
#include <string>
#include <string_view>
#include <variant>

#include "mamba/specs/error.hpp"
#include "mamba/specs/match_spec.hpp"

namespace mamba::specs
{
    class PackageInfo;

    /**
     * Represents a condition in a match spec for conditional dependencies.
     *
     * Supports AND/OR logic and nested conditions. Used to represent conditions
     * like "python >=3.10", "python <3.8 or pypy", "(a or b) and c", etc.
     *
     * Example usage:
     * - "dep; if python >=3.10" -> MatchSpecCondition with MatchSpecCondition_ containing "python
     * >=3.10"
     * - "dep; if python <3.8 or pypy" -> MatchSpecCondition with Or containing two
     * MatchSpecCondition_
     * - "dep; if (a or b) and c" -> MatchSpecCondition with And containing Or and
     * MatchSpecCondition_
     */
    class MatchSpecCondition
    {
    public:

        /**
         * A condition based on a MatchSpec (e.g., python >=3.10, __unix, __win).
         */
        struct MatchSpecCondition_
        {
            MatchSpec spec;

            [[nodiscard]] auto operator==(const MatchSpecCondition_& other) const -> bool;
        };

        /**
         * Logical AND of two conditions.
         */
        struct And
        {
            std::unique_ptr<MatchSpecCondition> left;
            std::unique_ptr<MatchSpecCondition> right;

            [[nodiscard]] auto operator==(const And& other) const -> bool;
        };

        /**
         * Logical OR of two conditions.
         */
        struct Or
        {
            std::unique_ptr<MatchSpecCondition> left;
            std::unique_ptr<MatchSpecCondition> right;

            [[nodiscard]] auto operator==(const Or& other) const -> bool;
        };

        using variant_type = std::variant<MatchSpecCondition_, std::unique_ptr<And>, std::unique_ptr<Or>>;

        /**
         * Construct from a simple MatchSpec condition.
         */
        explicit MatchSpecCondition(MatchSpecCondition_ cond);

        /**
         * Construct from an AND condition.
         */
        explicit MatchSpecCondition(std::unique_ptr<And> cond);

        /**
         * Construct from an OR condition.
         */
        explicit MatchSpecCondition(std::unique_ptr<Or> cond);

        /**
         * Copy constructor.
         */
        MatchSpecCondition(const MatchSpecCondition& other);

        /**
         * Copy assignment operator.
         */
        MatchSpecCondition& operator=(const MatchSpecCondition& other);

        /**
         * Move constructor.
         */
        MatchSpecCondition(MatchSpecCondition&&) = default;

        /**
         * Move assignment operator.
         */
        MatchSpecCondition& operator=(MatchSpecCondition&&) = default;

        /**
         * Parse a condition string (e.g., "python >=3.10", "a and b", "(a or b) and c").
         *
         * The input should be the part after "; if" in a conditional dependency.
         * For example, from "dep; if python >=3.10", pass "python >=3.10".
         */
        [[nodiscard]] static auto parse(std::string_view condition_str)
            -> expected_parse_t<MatchSpecCondition>;

        /**
         * Check if the condition is satisfied given a package.
         *
         * For MatchSpecCondition_, checks if the package matches the MatchSpec.
         * For And/Or, evaluates the logical combination.
         *
         * Note: This is a simplified check. Full evaluation requires the entire
         * environment context (all installed packages), which is handled at the solver level.
         */
        [[nodiscard]] auto contains(const PackageInfo& pkg) const -> bool;

        /**
         * Convert the condition to a string representation.
         *
         * Examples:
         * - "python >=3.10"
         * - "(python <3.8 or pypy)"
         * - "a and (b or c)"
         */
        [[nodiscard]] auto to_string() const -> std::string;

        /**
         * Equality comparison.
         */
        [[nodiscard]] auto operator==(const MatchSpecCondition& other) const -> bool;

        variant_type value;
    };
}

template <>
struct std::hash<mamba::specs::MatchSpecCondition>
{
    auto operator()(const mamba::specs::MatchSpecCondition& cond) const -> std::size_t;
};

template <>
struct std::hash<mamba::specs::MatchSpecCondition::MatchSpecCondition_>
{
    auto operator()(const mamba::specs::MatchSpecCondition::MatchSpecCondition_& cond) const
        -> std::size_t;
};

template <>
struct std::hash<mamba::specs::MatchSpecCondition::And>
{
    auto operator()(const mamba::specs::MatchSpecCondition::And& cond) const -> std::size_t;
};

template <>
struct std::hash<mamba::specs::MatchSpecCondition::Or>
{
    auto operator()(const mamba::specs::MatchSpecCondition::Or& cond) const -> std::size_t;
};

#endif
