// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "libmambapy.hpp"

PYBIND11_MODULE(core, m)
{
    mamba::version::bind_submodule(m.def_submodule("version"));
    mamba::bindings::bind_submodule(m.def_submodule("bindings"));
}
