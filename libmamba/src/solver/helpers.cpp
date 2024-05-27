// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "solver/helpers.hpp"

namespace mamba::solver
{
    auto find_new_python_in_solution(const Solution& solution
    ) -> std::optional<std::reference_wrapper<const specs::PackageInfo>>
    {
        auto out = std::optional<std::reference_wrapper<const specs::PackageInfo>>{};
        for_each_to_install(
            solution.actions,
            [&](const auto& pkg)
            {
                if (pkg.name == "python")
                {
                    out = std::cref(pkg);
                    return util::LoopControl::Break;
                }
                return util::LoopControl::Continue;
            }
        );
        return out;
    }

    auto python_binary_compatible(const specs::Version& older, const specs::Version& newer) -> bool
    {
        // Python binary compatibility is defined at the same MINOR level.
        return older.compatible_with(newer, /* level= */ 2);
    }
}
