// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <type_traits>

#include "mamba/specs/version_spec.hpp"

namespace mamba::specs
{

    /************************************
     *  VersionInterval Implementation  *
     ************************************/

    VersionInterval::VersionInterval(IntervalImpl&& interval) noexcept
        : m_interval{ std::move(interval) }
    {
    }

    auto VersionInterval::make_empty() -> VersionInterval
    {
        return VersionInterval(Empty{});
    }

    auto VersionInterval::make_free() -> VersionInterval
    {
        return VersionInterval(Free{});
    }

    auto VersionInterval::make_singleton(const Version& point) -> VersionInterval
    {
        return VersionInterval(Singleton{ point });
    }

    auto VersionInterval::make_singleton(Version&& point) -> VersionInterval
    {
        return VersionInterval(Singleton{ std::move(point) });
    }

    auto VersionInterval::make_lower_bounded(const Version& lb, Bound type) -> VersionInterval
    {
        return VersionInterval(LowerBounded{ lb, type });
    }

    auto VersionInterval::make_lower_bounded(Version&& lb, Bound type) -> VersionInterval
    {
        return VersionInterval(LowerBounded{ std::move(lb), type });
    }

    auto VersionInterval::make_upper_bounded(const Version& ub, Bound type) -> VersionInterval
    {
        return VersionInterval(UpperBounded{ ub, type });
    }

    auto VersionInterval::make_upper_bounded(Version&& ub, Bound type) -> VersionInterval
    {
        return VersionInterval(UpperBounded{ std::move(ub), type });
    }

    auto VersionInterval::make_bounded(const Version& lb, Bound ltype, const Version& ub, Bound utype)
        -> VersionInterval
    {
        return make_bounded(Version(lb), ltype, Version(ub), utype);
    }

    auto VersionInterval::make_bounded(Version&& lb, Bound ltype, Version&& ub, Bound utype)
        -> VersionInterval
    {
        if (lb < ub)
        {
            return VersionInterval(Bounded{ std::move(lb), ltype, std::move(ub), utype });
        }
        if ((ltype == Bound::Closed) && (utype == Bound::Closed) && (lb == ub))
        {
            return make_singleton(std::move(lb));
        }
        return make_empty();
    }

    auto VersionInterval::is_empty() const -> bool
    {
        return std::holds_alternative<Empty>(m_interval);
    }

    auto VersionInterval::is_free() const -> bool
    {
        return std::holds_alternative<Free>(m_interval);
    }

    auto VersionInterval::is_singleton() const -> bool
    {
        return std::holds_alternative<Singleton>(m_interval);
    }

    auto VersionInterval::is_lower_bounded() const -> bool
    {
        return !(is_free() || std::holds_alternative<UpperBounded>(m_interval));
    }

    auto VersionInterval::is_upper_bounded() const -> bool
    {
        return !(is_free() || std::holds_alternative<LowerBounded>(m_interval));
    }

    auto VersionInterval::is_bounded() const -> bool
    {
        return is_empty() || is_singleton() || std::holds_alternative<Bounded>(m_interval);
    }

    auto VersionInterval::is_lower_closed() const -> bool
    {
        return is_empty() || is_singleton()
               || (std::holds_alternative<LowerBounded>(m_interval)
                   && std::get<LowerBounded>(m_interval).ltype == Bound::Closed)
               || (std::holds_alternative<Bounded>(m_interval)
                   && std::get<Bounded>(m_interval).ltype == Bound::Closed);
    }

    auto VersionInterval::is_upper_closed() const -> bool
    {
        return is_empty() || is_singleton()
               || (std::holds_alternative<UpperBounded>(m_interval)
                   && std::get<UpperBounded>(m_interval).utype == Bound::Closed)
               || (std::holds_alternative<Bounded>(m_interval)
                   && std::get<Bounded>(m_interval).utype == Bound::Closed);
    }

    auto VersionInterval::is_closed() const -> bool
    {
        return is_lower_closed() && is_upper_closed();
    }

    auto VersionInterval::is_segment() const -> bool
    {
        // Closed implies bounded
        return is_closed();
    }

    auto VersionInterval::contains(const Version& point) const -> bool
    {
        return std::visit(
            [&point](const auto& itv) -> bool
            {
                using I = std::decay_t<decltype(itv)>;
                if constexpr (std::is_same_v<I, Empty>)
                {
                    return false;
                }
                else if constexpr (std::is_same_v<I, Free>)
                {
                    return true;
                }
                else if constexpr (std::is_same_v<I, Singleton>)
                {
                    return itv.point == point;
                }
                else if constexpr (std::is_same_v<I, LowerBounded>)
                {
                    return ((itv.ltype == Bound::Closed) && (itv.lower <= point))
                           || ((itv.ltype == Bound::Open) && (itv.lower < point));
                }
                else if constexpr (std::is_same_v<I, UpperBounded>)
                {
                    return ((itv.utype == Bound::Closed) && (point <= itv.upper))
                           || ((itv.utype == Bound::Open) && (point < itv.upper));
                }
                else if constexpr (std::is_same_v<I, Bounded>)
                {
                    return (  // The lower bound check
                               ((itv.ltype == Bound::Closed) && (itv.lower <= point))
                               || ((itv.ltype == Bound::Open) && (itv.lower < point))
                           )
                           && (  // The upper bound check
                               ((itv.utype == Bound::Closed) && (point <= itv.upper))
                               || ((itv.utype == Bound::Open) && (point < itv.upper))
                           );
                }
                assert(false);  // All cases are handled
                return false;
            },
            m_interval
        );
    }

    auto operator==(VersionInterval::Empty, VersionInterval::Empty) -> bool
    {
        return true;
    }

    auto operator==(VersionInterval::Free, VersionInterval::Free) -> bool
    {
        return true;
    }

    auto operator==(const VersionInterval::Singleton& lhs, const VersionInterval::Singleton& rhs)
        -> bool
    {
        return lhs.point == rhs.point;
    }

    auto
    operator==(const VersionInterval::LowerBounded& lhs, const VersionInterval::LowerBounded& rhs)
        -> bool
    {
        return (lhs.ltype == rhs.ltype) && (lhs.lower == rhs.lower);
    }

    auto
    operator==(const VersionInterval::UpperBounded& lhs, const VersionInterval::UpperBounded& rhs)

        -> bool
    {
        return (lhs.utype == rhs.utype) && (lhs.upper == rhs.upper);
    }

    auto operator==(const VersionInterval::Bounded& lhs, const VersionInterval::Bounded& rhs)

        -> bool
    {
        return (
            (lhs.utype == rhs.utype) && (lhs.ltype == rhs.ltype) && (lhs.upper == rhs.upper)
            && (lhs.lower == rhs.lower)
        );
    }

    auto operator==(const VersionInterval& lhs, const VersionInterval& rhs) -> bool
    {
        // returns false if the variant does not contain the same type
        return lhs.m_interval == rhs.m_interval;
    }

    auto operator!=(const VersionInterval& lhs, const VersionInterval& rhs) -> bool
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
}
