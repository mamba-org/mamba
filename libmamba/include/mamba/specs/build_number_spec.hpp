// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_BUILD_NUMBER_SPEC_HPP
#define MAMBA_SPECS_BUILD_NUMBER_SPEC_HPP

#include <array>
#include <functional>
#include <string_view>
#include <variant>

#include <fmt/format.h>

#include "mamba/specs/error.hpp"

namespace mamba::specs
{
    /**
     * A stateful unary boolean function on the integer space.
     */
    class BuildNumberPredicate
    {
    public:

        using BuildNumber = std::size_t;

        [[nodiscard]] static auto make_free() -> BuildNumberPredicate;
        [[nodiscard]] static auto make_equal_to(BuildNumber ver) -> BuildNumberPredicate;
        [[nodiscard]] static auto make_not_equal_to(BuildNumber ver) -> BuildNumberPredicate;
        [[nodiscard]] static auto make_greater(BuildNumber ver) -> BuildNumberPredicate;
        [[nodiscard]] static auto make_greater_equal(BuildNumber ver) -> BuildNumberPredicate;
        [[nodiscard]] static auto make_less(BuildNumber ver) -> BuildNumberPredicate;
        [[nodiscard]] static auto make_less_equal(BuildNumber ver) -> BuildNumberPredicate;

        /** Construct an free interval. */
        BuildNumberPredicate() = default;

        /**
         * True if the predicate contains the given build_number.
         */
        [[nodiscard]] auto contains(const BuildNumber& point) const -> bool;

        [[nodiscard]] auto to_string() const -> std::string;

    private:

        struct free_interval
        {
            auto operator()(const BuildNumber&, const BuildNumber&) const -> bool;
        };

        /**
         * Operator to compare with the stored build_number.
         */
        using BinaryOperator = std::variant<
            free_interval,
            std::equal_to<BuildNumber>,
            std::not_equal_to<BuildNumber>,
            std::greater<BuildNumber>,
            std::greater_equal<BuildNumber>,
            std::less<BuildNumber>,
            std::less_equal<BuildNumber>>;

        BuildNumber m_build_number = {};
        BinaryOperator m_operator = free_interval{};

        BuildNumberPredicate(BuildNumber ver, BinaryOperator op);

        friend auto equal(free_interval, free_interval) -> bool;
        friend auto operator==(const BuildNumberPredicate& lhs, const BuildNumberPredicate& rhs)
            -> bool;
        friend struct ::fmt::formatter<BuildNumberPredicate>;
    };

    auto operator==(const BuildNumberPredicate& lhs, const BuildNumberPredicate& rhs) -> bool;
    auto operator!=(const BuildNumberPredicate& lhs, const BuildNumberPredicate& rhs) -> bool;

    /**
     * Match a build number with a predicate.
     *
     * Conda does not implement expression for build numbers but they could be added similarly
     * to @ref VersionSpec.
     */
    class BuildNumberSpec
    {
    public:

        using BuildNumber = typename BuildNumberPredicate::BuildNumber;

        static constexpr std::string_view preferred_free_str = "=*";
        static constexpr std::array<std::string_view, 4> all_free_strs = { "", "*", "=*", "==*" };
        static constexpr std::string_view equal_str = "=";
        static constexpr std::string_view not_equal_str = "!=";
        static constexpr std::string_view greater_str = ">";
        static constexpr std::string_view greater_equal_str = ">=";
        static constexpr std::string_view less_str = "<";
        static constexpr std::string_view less_equal_str = "<=";

        [[nodiscard]] static auto parse(std::string_view str) -> expected_parse_t<BuildNumberSpec>;

        /** Construct BuildNumberSpec that match all versions. */
        BuildNumberSpec();

        explicit BuildNumberSpec(BuildNumberPredicate predicate) noexcept;

        /**
         * Returns whether the BuildNumberSpec is unconstrained.
         */
        [[nodiscard]] auto is_explicitly_free() const -> bool;

        /**
         * A string representation of the build number spec.
         *
         * May not always be the same as the parsed string (due to reconstruction) but reparsing
         * this string will give the same build number spec.
         */
        [[nodiscard]] auto to_string() const -> std::string;

        /**
         * True if the set described by the BuildNumberSpec contains the given version.
         */
        [[nodiscard]] auto contains(BuildNumber point) const -> bool;

        [[nodiscard]] auto operator==(const BuildNumberSpec& other) const -> bool = default;

        [[nodiscard]] auto operator!=(const BuildNumberSpec& other) const -> bool = default;

    private:

        BuildNumberPredicate m_predicate;

        friend struct ::fmt::formatter<BuildNumberSpec>;
    };

    namespace build_number_spec_literals
    {
        auto operator""_bs(const char* str, std::size_t len) -> BuildNumberSpec;
    }
}

template <>
struct fmt::formatter<mamba::specs::BuildNumberPredicate>
{
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        // make sure that range is empty
        if (ctx.begin() != ctx.end() && *ctx.begin() != '}')
        {
            throw format_error("invalid format");
        }
        return ctx.begin();
    }

    auto format(const ::mamba::specs::BuildNumberPredicate& pred, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct fmt::formatter<mamba::specs::BuildNumberSpec>
{
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        // make sure that range is empty
        if (ctx.begin() != ctx.end() && *ctx.begin() != '}')
        {
            throw format_error("invalid format");
        }
        return ctx.begin();
    }

    auto format(const ::mamba::specs::BuildNumberSpec& spec, format_context& ctx) const
        -> format_context::iterator;
};

template <>
struct std::hash<mamba::specs::BuildNumberSpec>
{
    auto operator()(const mamba::specs::BuildNumberSpec& spec) const -> std::size_t;
};

#endif
