// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "bindings.hpp"

PYBIND11_MODULE(bindings, m)
{
    mambapy::bind_submodule_specs(m.def_submodule("specs"));
    mambapy::bind_submodule_solver_libsolv(m.def_submodule("solver").def_submodule("libsolv"));
    mambapy::bind_submodule_legacy(m.def_submodule("legacy"));
}
