// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>

#include "mamba/core/db_utils.hpp"

namespace mamba
{
    namespace
    {
        void update_latest(std::optional<specs::PackageInfo>& out, specs::PackageInfo&& pkg)
        {
            if (!out || PkgInfoCmp()(&*out, &pkg))
            {
                out = std::move(pkg);
            }
        }
    }

    auto database_latest_package(solver::libsolv::Database& database, specs::MatchSpec spec)
        -> std::optional<specs::PackageInfo>
    {
        auto out = std::optional<specs::PackageInfo>();
        database.for_each_package_matching(
            spec,
            [&](auto pkg) { update_latest(out, std::move(pkg)); }
        );
        return out;
    };

    auto database_latest_package_matching_all(
        solver::libsolv::Database& database,
        std::span<const DepEntry> entries
    ) -> std::optional<specs::PackageInfo>
    {
        assert(!entries.empty());

        auto out = std::optional<specs::PackageInfo>();
        database.for_each_package_matching(
            entries.front().ms,
            [&](auto pkg)
            {
                if (std::all_of(
                        entries.begin() + 1,
                        entries.end(),
                        [&](const auto& entry) { return entry.ms.contains_except_channel(pkg); }
                    ))
                {
                    update_latest(out, std::move(pkg));
                }
            }
        );
        return out;
    };
}
