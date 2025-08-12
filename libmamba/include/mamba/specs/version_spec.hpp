// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_VERSION_SPEC_HPP
#define MAMBA_SPECS_VERSION_SPEC_HPP

#include <array>
#include <functional>
#include <string_view>
#include <variant>

#include <fmt/format.h>

#include "mamba/specs/error.hpp"
#include "mamba/specs/version.hpp"
#include "mamba/util/flat_bool_expr_tree.hpp"

namespace mamba::specs
{
    /**
     * A stateful unary boolean function on the Version space.
     */
    class VersionPredicate
    {
    public:

        [[nodiscard]] static auto make_free() -> VersionPredicate;
        [[nodiscard]] static auto make_equal_to(Version ver) -> VersionPredicate;
        [[nodiscard]] static auto make_not_equal_to(Version ver) -> VersionPredicate;
        [[nodiscard]] static auto make_greater(Version ver) -> VersionPredicate;
        [[nodiscard]] static auto make_greater_equal(Version ver) -> VersionPredicate;
        [[nodiscard]] static auto make_less(Version ver) -> VersionPredicate;
        [[nodiscard]] static auto make_less_equal(Version ver) -> VersionPredicate;
        [[nodiscard]] static auto make_starts_with(Version ver) -> VersionPredicate;
        [[nodiscard]] static auto make_not_starts_with(Version ver) -> VersionPredicate;
        [[nodiscard]] static auto make_compatible_with(Version ver, std::size_t level)
            -> VersionPredicate;
        [[nodiscard]] static auto make_version_glob(Version pattern) -> VersionPredicate;
        [[nodiscard]] static auto make_not_version_glob(Version pattern) -> VersionPredicate;

        /** Construct an free interval. */
        VersionPredicate() = default;

        /**
         * True if the predicate contains the given version.
         */
        [[nodiscard]] auto contains(const Version& point) const -> bool;

        /**
         * True if it contains a glob or negative glob expression.
         *
         * Does not return true for predicates that could be written as globs but are not.
         */
        [[nodiscard]] auto has_glob() const -> bool;

        /**
         * True if the predicate is a simple operator.
         *
         * Simple operators are ``=``, ``!=``, ``<``, ``<=``, ``>``, ``>=``.
         */
        [[nodiscard]] auto is_classic_operator() const -> bool;

        [[nodiscard]] auto to_string() const -> std::string;

        /**
         * An alternative string representation of the version spec.
         *
         * Attempts to be compatible with conda-build/libsolv.
         */
        [[nodiscard]] auto to_string_conda_build() const -> std::string;

    private:

        struct free_interval
        {
            auto operator()(const Version&, const Version&) const -> bool;
        };

        struct starts_with
        {
            auto operator()(const Version&, const Version&) const -> bool;
        };

        struct not_starts_with
        {
            auto operator()(const Version&, const Version&) const -> bool;
        };

        struct compatible_with
        {
            std::size_t level;
            auto operator()(const Version&, const Version&) const -> bool;
        };

        struct version_glob
        {
            auto operator()(const Version&, const Version&) const -> bool;
        };

        struct not_version_glob
        {
            auto operator()(const Version&, const Version&) const -> bool;
        };

        /**
         * Operator to compare with the stored version.
         *
         * We could store arbitrary binary operators, but since this is tightly coupled with
         * ``VersionSpec`` parsing (hence not user-extensible), and performance-sensitive,
         * we choose an ``std::variant`` for dynamic dispatch.
         * An alternative could be a type-erased wrapper with local storage.
         *
         * Not alternatives (``not_starts_with``, ``not_version_glob``) could also be implemented
         * as a not operator in VersionSpec rather than a predicate, but they are used often enough
         * to deserve their specialization.
         */
        using BinaryOperator = std::variant<
            free_interval,
            std::equal_to<Version>,
            std::not_equal_to<Version>,
            std::greater<Version>,
            std::greater_equal<Version>,
            std::less<Version>,
            std::less_equal<Version>,
            starts_with,
            not_starts_with,
            compatible_with,
            not_version_glob,
            version_glob>;

        // Originally, with only stateless operators, it made sense to have the version factored
        // in this class' attributes. However, with additions of variants that use the version
        // for a different meaning (version_glob, compatible_with), or not at all (free interval),
        // it would make sense to move it to each individual class for better scoping (e.g. see
        // this class' operator==).
        Version m_version = {};
        BinaryOperator m_operator = free_interval{};

        VersionPredicate(Version ver, BinaryOperator op);

        friend auto operator==(free_interval, free_interval) -> bool;
        friend auto operator==(starts_with, starts_with) -> bool;
        friend auto operator==(not_starts_with, not_starts_with) -> bool;
        friend auto operator==(compatible_with, compatible_with) -> bool;
        friend auto operator==(version_glob, version_glob) -> bool;
        friend auto operator==(not_version_glob, not_version_glob) -> bool;
        friend auto operator==(const VersionPredicate& lhs, const VersionPredicate& rhs) -> bool;
        friend struct ::fmt::formatter<VersionPredicate>;
    };

    auto operator==(const VersionPredicate& lhs, const VersionPredicate& rhs) -> bool;
    auto operator!=(const VersionPredicate& lhs, const VersionPredicate& rhs) -> bool;

    /**
     * Represent a set of versions.
     *
     * Internally, a VersionSpec is a binary expression tree of union (or) or intersections (and)
     * of the sets represented by VersionPredicate.
     *
     * The VersionSpec can itself be considered a complex predicate on the space of Version.
     *
     * Due to the complex nature of the expression system (comutativity, associativity, etc.), there
     * is no easy way to say if two VersionSpecs are equal.
     */
    class VersionSpec
    {
    public:

        using tree_type = util::flat_bool_expr_tree<VersionPredicate>;

        static constexpr char and_token = ',';
        static constexpr char or_token = '|';
        static constexpr char left_parenthesis_token = '(';
        static constexpr char right_parenthesis_token = ')';

        static constexpr std::string_view preferred_free_str = "=*";
        static constexpr std::array<std::string_view, 4> all_free_strs = { "", "*", "=*", "==*" };
        static constexpr std::string_view starts_with_str = "=";
        static constexpr std::string_view equal_str = "==";
        static constexpr std::string_view not_equal_str = "!=";
        static constexpr std::string_view greater_str = ">";
        static constexpr std::string_view greater_equal_str = ">=";
        static constexpr std::string_view less_str = "<";
        static constexpr std::string_view less_equal_str = "<=";
        static constexpr std::string_view compatible_str = "~=";
        static constexpr std::string_view glob_suffix_str = ".*";
        static constexpr std::string_view glob_pattern_str = "*";

        [[nodiscard]] static auto parse(std::string_view str) -> expected_parse_t<VersionSpec>;

        /**
         * Create a Version spec with a single predicate in the expression.
         */
        [[nodiscard]] static auto from_predicate(VersionPredicate pred) -> VersionSpec;

        /** Construct VersionSpec that match all versions. */
        VersionSpec() = default;
        explicit VersionSpec(tree_type&& tree) noexcept;

        /**
         * Returns whether the VersionSpec is unconstrained.
         *
         * Due to the complex nature of VersionSpec expressions, it is not always easy to know
         * whether a complex expression can be simplified to the unconstrained one.
         * This functions only handles the trivial cases.
         */
        [[nodiscard]] auto is_explicitly_free() const -> bool;

        /**
         * True if it contains a glob or negative glob expression anywhere in the tree.
         *
         * Does not return true for predicates that could be written as globs but are not.
         */
        [[nodiscard]] auto has_glob() const -> bool;

        /**
         * True if the version spec is an expression composed of simple operators.
         *
         * @see VersionPredicate::is_classic_operator
         */
        [[nodiscard]] auto is_classic_operator_expression() const -> bool;

        /**
         * A string representation of the version spec.
         *
         * May not always be the same as the parsed string (due to reconstruction) but reparsing
         * this string will give the same version spec.
         */
        [[nodiscard]] auto to_string() const -> std::string;

        /**
         * An alternative string representation of the version spec.
         *
         * Attempts to be compatible with conda-build/libsolv.
         */
        [[nodiscard]] auto to_string_conda_build() const -> std::string;

        /**
         * True if the set described by the VersionSpec contains the given version.
         */
        [[nodiscard]] auto contains(const Version& point) const -> bool;

        /**
         * Return the size of the boolean expression tree.
         */
        [[nodiscard]] auto expression_size() const -> std::size_t;

        [[nodiscard]] auto operator==(const VersionSpec& other) const -> bool
        {
            return m_tree == other.m_tree;
        }

        [[nodiscard]] auto operator!=(const VersionSpec& other) const -> bool
        {
            return !(*this == other);
        }

    private:

        tree_type m_tree;

        friend struct ::fmt::formatter<VersionSpec>;
    };

    namespace version_spec_literals
    {
        auto operator""_vs(const char* str, std::size_t len) -> VersionSpec;
    }
}

template <>
struct fmt::formatter<mamba::specs::VersionPredicate>
{
    /**
     * Change the representation of some predicates not understood by conda-build/libsolv.
     */
    bool conda_build_form = false;

    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        const auto end = ctx.end();
        for (auto it = ctx.begin(); it != end; ++it)
        {
            if (*it == 'b')
            {
                conda_build_form = true;
                return ++it;
            }
        }
        return ctx.begin();
    }

    auto format(const ::mamba::specs::VersionPredicate& pred, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct fmt::formatter<mamba::specs::VersionSpec>
{
    /**
     * Change the representation of some predicates not understood by conda-build/libsolv.
     */
    bool conda_build_form = false;

    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        const auto end = ctx.end();
        for (auto it = ctx.begin(); it != end; ++it)
        {
            if (*it == 'b')
            {
                conda_build_form = true;
                return ++it;
            }
        }
        return ctx.begin();
    }

    auto format(const ::mamba::specs::VersionSpec& spec, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct std::hash<mamba::specs::VersionPredicate>
{
    auto operator()(const mamba::specs::VersionPredicate& pred) const -> std::size_t;
};

template <>
struct std::hash<mamba::specs::VersionSpec>
{
    auto operator()(const mamba::specs::VersionSpec& spec) const -> std::size_t;
};

#endif
