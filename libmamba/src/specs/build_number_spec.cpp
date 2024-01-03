// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <charconv>
#include <stdexcept>

#include <fmt/format.h>

#include "mamba/specs/build_number_spec.hpp"
#include "mamba/util/string.hpp"

namespace mamba::specs
{
    /*********************************************************
     *  BuildNumberPredicate BinaryOperators Implementation  *
     *********************************************************/

    auto BuildNumberPredicate::free_interval::operator()(const BuildNumber&, const BuildNumber&) const
        -> bool
    {
        return true;
    }

    auto equal(BuildNumberPredicate::free_interval, BuildNumberPredicate::free_interval) -> bool
    {
        return true;
    }

    namespace
    {
        // Cannot specialize operator== in std namespace (UB) so we have to reinvent the wheel.

        auto
        equal(std::equal_to<BuildNumberPredicate::BuildNumber>, std::equal_to<BuildNumberPredicate::BuildNumber>)
            -> bool
        {
            return true;
        }

        auto
        equal(std::not_equal_to<BuildNumberPredicate::BuildNumber>, std::not_equal_to<BuildNumberPredicate::BuildNumber>)
            -> bool
        {
            return true;
        }

        auto
        equal(std::greater<BuildNumberPredicate::BuildNumber>, std::greater<BuildNumberPredicate::BuildNumber>)
            -> bool
        {
            return true;
        }

        auto
        equal(std::greater_equal<BuildNumberPredicate::BuildNumber>, std::greater_equal<BuildNumberPredicate::BuildNumber>)
            -> bool
        {
            return true;
        }

        auto
        equal(std::less<BuildNumberPredicate::BuildNumber>, std::less<BuildNumberPredicate::BuildNumber>)
            -> bool
        {
            return true;
        }

        auto
        equal(std::less_equal<BuildNumberPredicate::BuildNumber>, std::less_equal<BuildNumberPredicate::BuildNumber>)
            -> bool
        {
            return true;
        }

        template <typename U, typename... T>
        auto equal_with(const std::variant<T...>& lhs, const std::variant<T...>& rhs) -> bool
        {
            return std::holds_alternative<U>(lhs) && std::holds_alternative<U>(rhs)
                   && equal(std::get<U>(lhs), std::get<U>(rhs));
        }

        template <typename... T>
        auto equal(const std::variant<T...>& lhs, const std::variant<T...>& rhs) -> bool
        {
            return (equal_with<T>(lhs, rhs) || ...);
        }
    }

    /*****************************************
     *  BuildNumberPredicate Implementation  *
     *****************************************/

    auto BuildNumberPredicate::contains(const BuildNumber& point) const -> bool
    {
        return std::visit([&](const auto& op) { return op(point, m_build_number); }, m_operator);
    }

    auto BuildNumberPredicate::make_free() -> BuildNumberPredicate
    {
        return BuildNumberPredicate({}, free_interval{});
    }

    auto BuildNumberPredicate::make_equal_to(BuildNumber ver) -> BuildNumberPredicate
    {
        return BuildNumberPredicate(std::move(ver), std::equal_to<BuildNumber>{});
    }

    auto BuildNumberPredicate::make_not_equal_to(BuildNumber ver) -> BuildNumberPredicate
    {
        return BuildNumberPredicate(std::move(ver), std::not_equal_to<BuildNumber>{});
    }

    auto BuildNumberPredicate::make_greater(BuildNumber ver) -> BuildNumberPredicate
    {
        return BuildNumberPredicate(std::move(ver), std::greater<BuildNumber>{});
    }

    auto BuildNumberPredicate::make_greater_equal(BuildNumber ver) -> BuildNumberPredicate
    {
        return BuildNumberPredicate(std::move(ver), std::greater_equal<BuildNumber>{});
    }

    auto BuildNumberPredicate::make_less(BuildNumber ver) -> BuildNumberPredicate
    {
        return BuildNumberPredicate(std::move(ver), std::less<BuildNumber>{});
    }

    auto BuildNumberPredicate::make_less_equal(BuildNumber ver) -> BuildNumberPredicate
    {
        return BuildNumberPredicate(std::move(ver), std::less_equal<BuildNumber>{});
    }

    auto BuildNumberPredicate::str() const -> std::string
    {
        return fmt::format("{}", *this);
    }

    BuildNumberPredicate::BuildNumberPredicate(BuildNumber ver, BinaryOperator op)
        : m_build_number(std::move(ver))
        , m_operator(std::move(op))
    {
    }

    auto operator==(const BuildNumberPredicate& lhs, const BuildNumberPredicate& rhs) -> bool
    {
        return (lhs.m_build_number == rhs.m_build_number) && equal(lhs.m_operator, rhs.m_operator);
    }

    auto operator!=(const BuildNumberPredicate& lhs, const BuildNumberPredicate& rhs) -> bool
    {
        return !(lhs == rhs);
    }
}

auto
fmt::formatter<mamba::specs::BuildNumberPredicate>::parse(format_parse_context& ctx)
    -> decltype(ctx.begin())
{
    // make sure that range is empty
    if (ctx.begin() != ctx.end() && *ctx.begin() != '}')
    {
        throw format_error("invalid format");
    }
    return ctx.begin();
}

auto
fmt::formatter<mamba::specs::BuildNumberPredicate>::format(
    const ::mamba::specs::BuildNumberPredicate& pred,
    format_context& ctx
) -> decltype(ctx.out())
{
    using BuildNumberPredicate = typename mamba::specs::BuildNumberPredicate;
    using BuildNumber = typename BuildNumberPredicate::BuildNumber;
    using BuildNumberSpec = typename mamba::specs::BuildNumberSpec;

    auto out = ctx.out();
    std::visit(
        [&](const auto& op)
        {
            using Op = std::decay_t<decltype(op)>;
            if constexpr (std::is_same_v<Op, BuildNumberPredicate::free_interval>)
            {
                out = fmt::format_to(out, "{}", BuildNumberSpec::prefered_free_str);
            }
            if constexpr (std::is_same_v<Op, std::equal_to<BuildNumber>>)
            {
                out = fmt::format_to(out, "{}{}", BuildNumberSpec::equal_str, pred.m_build_number);
            }
            if constexpr (std::is_same_v<Op, std::not_equal_to<BuildNumber>>)
            {
                out = fmt::format_to(out, "{}{}", BuildNumberSpec::not_equal_str, pred.m_build_number);
            }
            if constexpr (std::is_same_v<Op, std::greater<BuildNumber>>)
            {
                out = fmt::format_to(out, "{}{}", BuildNumberSpec::greater_str, pred.m_build_number);
            }
            if constexpr (std::is_same_v<Op, std::greater_equal<BuildNumber>>)
            {
                out = fmt::format_to(out, "{}{}", BuildNumberSpec::greater_equal_str, pred.m_build_number);
            }
            if constexpr (std::is_same_v<Op, std::less<BuildNumber>>)
            {
                out = fmt::format_to(out, "{}{}", BuildNumberSpec::less_str, pred.m_build_number);
            }
            if constexpr (std::is_same_v<Op, std::less_equal<BuildNumber>>)
            {
                out = fmt::format_to(out, "{}{}", BuildNumberSpec::less_equal_str, pred.m_build_number);
            }
        },
        pred.m_operator
    );
    return out;
}

namespace mamba::specs
{
    auto BuildNumberSpec::parse(std::string_view str) -> BuildNumberSpec
    {
        str = util::strip(str);
        if (std::find(all_free_strs.cbegin(), all_free_strs.cend(), str) != all_free_strs.cend())
        {
            return BuildNumberSpec(BuildNumberPredicate::make_free());
        }

        auto [op_str, num_str] = util::lstrip_if_parts(str, [](char c) { return !util::is_digit(c); });
        op_str = util::strip(op_str);

        auto build_number = BuildNumber{};
        const auto [ptr, ec] = std::from_chars(
            num_str.data(),
            num_str.data() + num_str.size(),
            build_number
        );
        if ((ec != std::errc()) || (ptr != (num_str.data() + num_str.size())))
        {
            throw std::invalid_argument(
                fmt::format(R"(Expected number in build number spec "{}")", str)
            );
        }

        if (op_str.empty())
        {
            return BuildNumberSpec(BuildNumberPredicate::make_equal_to(build_number));
        }
        if (op_str == equal_str)
        {
            return BuildNumberSpec(BuildNumberPredicate::make_equal_to(build_number));
        }
        if (op_str == not_equal_str)
        {
            return BuildNumberSpec(BuildNumberPredicate::make_not_equal_to(build_number));
        }
        if (op_str == greater_str)
        {
            return BuildNumberSpec(BuildNumberPredicate::make_greater(build_number));
        }
        if (op_str == greater_equal_str)
        {
            return BuildNumberSpec(BuildNumberPredicate::make_greater_equal(build_number));
        }
        if (op_str == less_str)
        {
            return BuildNumberSpec(BuildNumberPredicate::make_less(build_number));
        }
        if (op_str == less_equal_str)
        {
            return BuildNumberSpec(BuildNumberPredicate::make_less_equal(build_number));
        }

        throw std::invalid_argument(fmt::format(R"(Invalid build number operator in"{}")", str));
    }

    BuildNumberSpec::BuildNumberSpec()
        : m_predicate(BuildNumberPredicate::make_free())
    {
    }

    BuildNumberSpec::BuildNumberSpec(BuildNumberPredicate predicate) noexcept
        : m_predicate(std::move(predicate))
    {
    }

    auto BuildNumberSpec::is_explicitly_free() const -> bool
    {
        return m_predicate == BuildNumberPredicate::make_free();
    }

    auto BuildNumberSpec::str() const -> std::string
    {
        return fmt::format("{}", *this);
    }

    auto BuildNumberSpec::contains(BuildNumber point) const -> bool
    {
        return m_predicate.contains(point);
    }

    namespace build_number_spec_literals
    {
        auto operator""_bs(const char* str, std::size_t len) -> BuildNumberSpec
        {
            return BuildNumberSpec::parse({ str, len });
        }
    }
}

auto
fmt::formatter<mamba::specs::BuildNumberSpec>::parse(format_parse_context& ctx)
    -> decltype(ctx.begin())
{
    // make sure that range is empty
    if (ctx.begin() != ctx.end() && *ctx.begin() != '}')
    {
        throw format_error("invalid format");
    }
    return ctx.begin();
}

auto
fmt::formatter<mamba::specs::BuildNumberSpec>::format(
    const ::mamba::specs::BuildNumberSpec& spec,
    format_context& ctx
) -> decltype(ctx.out())
{
    return fmt::format_to(ctx.out(), "{}", spec.m_predicate);
}
