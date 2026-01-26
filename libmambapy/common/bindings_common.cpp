// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "../bindings/bindings.hpp"

PYBIND11_MODULE(libmambapy_common, m)
{
    // Create submodules matching the aggregated module structure
    mambapy::bind_submodule_utils(m.def_submodule("utils"));
    mambapy::bind_submodule_specs(m.def_submodule("specs"));
}
