// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <pybind11/pybind11.h>

#include "mamba/specs/channel_spec.hpp"

#include "bindings.hpp"
#include "flat_set_caster.hpp"


namespace mambapy
{
    void bind_submodule_specs(pybind11::module_ m)
    {
        namespace py = pybind11;
        using namespace mamba::specs;

        py::class_<ChannelSpec>(m, "ChannelSpec");
    }
}
