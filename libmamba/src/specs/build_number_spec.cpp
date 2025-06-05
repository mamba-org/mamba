// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <charconv>

#include <fmt/format.h>

#include "mamba/specs/build_number_spec.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/tuple_hash.hpp"

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

    auto BuildNumberPredicate::to_string() const -> std::string
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
        return (lhs.m_build_number == rhs.m_build_number)
               // WARNING: the following is intended as an easy substitute for variant comparison.
               // It is not possible to specialize ``operator==`` for std::equal_to and the
               // like in ``namespace std`` (undefined behaviour).
               // But since none of the variant types currently have any attributes, simply
               // comparing the types does the same as the variant ``operator==``.
               && (lhs.m_operator.index() == rhs.m_operator.index());
    }

    auto operator!=(const BuildNumberPredicate& lhs, const BuildNumberPredicate& rhs) -> bool
    {
        return !(lhs == rhs);
    }
}

auto
fmt::formatter<mamba::specs::BuildNumberPredicate>::format(
    const ::mamba::specs::BuildNumberPredicate& pred,
    format_context& ctx
) const -> format_context::iterator
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
                out = fmt::format_to(out, "{}", BuildNumberSpec::preferred_free_str);
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
    auto BuildNumberSpec::parse(std::string_view str) -> expected_parse_t<BuildNumberSpec>
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
            return make_unexpected_parse(
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

        return make_unexpected_parse(fmt::format(R"(Invalid build number operator in"{}")", str));
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

    auto BuildNumberSpec::to_string() const -> std::string
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
            return BuildNumberSpec::parse({ str, len })
                .or_else([](ParseError&& err) { throw std::move(err); })
                .value();
        }
    }
}

auto
fmt::formatter<mamba::specs::BuildNumberSpec>::format(
    const ::mamba::specs::BuildNumberSpec& spec,
    format_context& ctx
) const -> decltype(ctx.out())
{
    return fmt::format_to(ctx.out(), "{}", spec.m_predicate);
}

auto
std::hash<mamba::specs::BuildNumberSpec>::operator()(const mamba::specs::BuildNumberSpec& spec) const
    -> std::size_t
{
    return mamba::util::hash_vals(spec.to_string());
}
