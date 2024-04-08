// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "bindings.hpp"

PYBIND11_MODULE(bindings, m)
{
    mambapy::bind_submodule_utils(m.def_submodule("utils"));
    mambapy::bind_submodule_specs(m.def_submodule("specs"));
    auto solver_submodule = m.def_submodule("solver");
    mambapy::bind_submodule_solver(solver_submodule);
    mambapy::bind_submodule_solver_libsolv(solver_submodule.def_submodule("libsolv"));
    mambapy::bind_submodule_legacy(m.def_submodule("legacy"));
}
