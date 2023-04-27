// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_VERSION_SPEC_HPP
#define MAMBA_SPECS_VERSION_SPEC_HPP

#include <stdexcept>
#include <variant>

#include "mamba/specs/version.hpp"


namespace mamba::specs
{
    class VersionInterval
    {
    public:

        enum struct Bound
        {
            Open,
            Closed
        };

        static auto make_empty() -> VersionInterval;
        static auto make_free() -> VersionInterval;
        static auto make_singleton(const Version& point) -> VersionInterval;
        static auto make_singleton(Version&& point) -> VersionInterval;
        static auto make_lower_bounded(const Version& lb, Bound type) -> VersionInterval;
        static auto make_lower_bounded(Version&& lb, Bound type) -> VersionInterval;
        static auto make_upper_bounded(const Version& ub, Bound type) -> VersionInterval;
        static auto make_upper_bounded(Version&& ub, Bound type) -> VersionInterval;
        static auto make_bounded(const Version& lb, Bound ltype, const Version& ub, Bound utype)
            -> VersionInterval;
        static auto make_bounded(Version&& lb, Bound ltype, Version&& ub, Bound utype)
            -> VersionInterval;

        /** Construct an empty interval. */
        VersionInterval() = default;

        [[nodiscard]] auto is_empty() const -> bool;
        [[nodiscard]] auto is_free() const -> bool;
        [[nodiscard]] auto is_singleton() const -> bool;
        [[nodiscard]] auto is_lower_bounded() const -> bool;
        [[nodiscard]] auto is_upper_bounded() const -> bool;
        [[nodiscard]] auto is_bounded() const -> bool;
        [[nodiscard]] auto is_lower_closed() const -> bool;
        [[nodiscard]] auto is_upper_closed() const -> bool;
        [[nodiscard]] auto is_closed() const -> bool;
        [[nodiscard]] auto is_segment() const -> bool;

        [[nodiscard]] auto contains(const Version& point) const -> bool;

    private:

        struct Empty
        {
        };

        struct Free
        {
        };

        struct Singleton
        {
            Version point;
        };

        struct LowerBounded
        {
            Version lower;
            Bound ltype;
        };

        struct UpperBounded
        {
            Version upper;
            Bound utype;
        };

        /**
         * A non degenerate or empty interval.
         *
         * Requires ``lower`` < ``upper``.
         */
        struct Bounded
        {
            Version lower;
            Bound ltype;
            Version upper;
            Bound utype;
        };

        using IntervalImpl = std::variant<Empty, Free, Singleton, LowerBounded, UpperBounded, Bounded>;

        friend auto operator==(Empty lhs, Empty rhs) -> bool;
        friend auto operator==(Free lhs, Free rhs) -> bool;
        friend auto operator==(const Singleton& lhs, const Singleton& rhs) -> bool;
        friend auto operator==(const LowerBounded& lhs, const LowerBounded& rhs) -> bool;
        friend auto operator==(const UpperBounded& lhs, const UpperBounded& rhs) -> bool;
        friend auto operator==(const Bounded& lhs, const Bounded& rhs) -> bool;
        friend auto operator==(const VersionInterval& lhs, const VersionInterval& rhs) -> bool;
        friend auto operator!=(const VersionInterval& lhs, const VersionInterval& rhs) -> bool;

        /** Construct an empty interval. */
        VersionInterval(IntervalImpl&& interval) noexcept;

        IntervalImpl m_interval = Empty{};
    };

    auto operator==(const VersionInterval& lhs, const VersionInterval& rhs) -> bool;
    auto operator!=(const VersionInterval& lhs, const VersionInterval& rhs) -> bool;
}

#endif
