// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "bindings.hpp"

PYBIND11_MODULE(bindings, m)
{
    mambapy::legacy::bind_submodule(m.def_submodule("legacy"));
}
