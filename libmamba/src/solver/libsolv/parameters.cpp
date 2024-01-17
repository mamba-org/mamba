// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <tuple>

#include "mamba/solver/libsolv/parameters.hpp"

namespace mamba::solver::libsolv
{
    namespace
    {
        auto attrs(const Priorities& p)
        {
            return std::tie(p.priority, p.subpriority);
        }
    }

    auto operator==(const Priorities& lhs, const Priorities& rhs) -> bool
    {
        return attrs(lhs) == attrs(rhs);
    }

    auto operator!=(const Priorities& lhs, const Priorities& rhs) -> bool
    {
        return !(lhs == rhs);
    }
}
