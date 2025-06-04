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
        // Obscure warning of a null dereference happening inside std::string
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif

        auto packages = solution.packages_to_install();
        auto it = std::ranges::find_if(packages, [](const auto& pkg) { return pkg.name == "python"; });
        if (it != packages.end())
        {
            return std::cref(*it);
        }
        return std::nullopt;

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    }

    auto python_binary_compatible(const specs::Version& older, const specs::Version& newer) -> bool
    {
        // Python binary compatibility is defined at the same MINOR level.
        return older.compatible_with(newer, /* level= */ 2);
    }
}
