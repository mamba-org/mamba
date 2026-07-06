// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <ranges>

#include "solver/helpers.hpp"

namespace mamba::solver
{
    auto find_new_python_in_solution(const Solution& solution)
        -> std::optional<std::reference_wrapper<const specs::PackageInfo>>
    {
        auto packages = solution.packages_to_install();
        auto it = std::ranges::find_if(packages, [](const auto& pkg) { return pkg.name == "python"; });
        if (it != packages.end())
        {
            return std::cref(*it);
        }
        return std::nullopt;
    }

    auto python_binary_compatible(const specs::Version& a, const specs::Version& b) -> bool
    {
        // Python binary compatibility is defined at the same major + minor
        // level. This matches conda's noarch:python relink gate in
        // conda/core/solve.py, which compares
        //   get_major_minor_version(prev) != get_major_minor_version(curr)
        // and is symmetric across upgrade/downgrade.
        const auto& a_ver = a.version();
        const auto& b_ver = b.version();
        return a.epoch() == b.epoch()                     //
               && a_ver.size() >= 2 && b_ver.size() >= 2  //
               && a_ver[0] == b_ver[0]                    //
               && a_ver[1] == b_ver[1];
    }
}
