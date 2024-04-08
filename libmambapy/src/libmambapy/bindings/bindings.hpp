// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef LIBMAMBAPY_BINDINGS_HPP
#define LIBMAMBAPY_BINDINGS_HPP

#include <pybind11/pybind11.h>

namespace mambapy
{
    void bind_submodule_utils(pybind11::module_ m);
    void bind_submodule_specs(pybind11::module_ m);
    void bind_submodule_solver(pybind11::module_ m);
    void bind_submodule_solver_libsolv(pybind11::module_ m);
    void bind_submodule_legacy(pybind11::module_ m);
}
#endif
