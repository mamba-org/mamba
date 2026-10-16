// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_DB_UTILS_HPP
#define MAMBA_CORE_DB_UTILS_HPP

#include <optional>
#include <span>
#include <string_view>
#include <tuple>

#include "mamba/solver/libsolv/database.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/specs/version.hpp"

namespace mamba
{
    struct PkgInfoCmp
    {
        auto operator()(const specs::PackageInfo* lhs, const specs::PackageInfo* rhs) const -> bool
        {
            auto attrs = [](const auto& pkg)
            {
                return std::tuple<decltype(pkg.name) const&, specs::Version>(
                    pkg.name,
                    // Failed parsing last
                    specs::Version::parse(pkg.version).value_or(specs::Version())
                );
            };
            return attrs(*lhs) < attrs(*rhs);
        }
    };

    struct DepEntry
    {
        std::string_view dep_str;
        specs::MatchSpec ms;
    };

    auto database_latest_package(solver::libsolv::Database& database, specs::MatchSpec spec)
        -> std::optional<specs::PackageInfo>;

    // Find the latest package matching all specs in the group.
    // Uses the first spec to find candidates via libsolv, then filters
    // by all remaining specs using `contains_except_channel`.
    auto database_latest_package_matching_all(
        solver::libsolv::Database& database,
        std::span<const DepEntry> entries
    ) -> std::optional<specs::PackageInfo>;
}
#endif
