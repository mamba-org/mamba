// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLVER_LIBSOLV_PARAMETERS_HPP
#define MAMBA_SOLVER_LIBSOLV_PARAMETERS_HPP

namespace mamba::solver::libsolv
{
    enum class RepodataParser
    {
        Mamba,
        Libsolv,
    };

    enum class LibsolvCache : bool
    {
        No = false,
        Yes = true,
    };

    enum class PipAsPythonDependency : bool
    {
        No = false,
        Yes = true,
    };

    struct Priorities
    {
        using value_type = int;

        value_type priority = 0;
        value_type subpriority = 0;
    };
}
#endif
