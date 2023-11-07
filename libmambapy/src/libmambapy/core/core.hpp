// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef LIBMAMBAPY_HPP
#define LIBMAMBAPY_HPP

#include <pybind11/pybind11.h>

namespace mambapy
{
    namespace version
    {
        void bind_submodule(pybind11::module_ m);
    }

    namespace bindings
    {
        void bind_submodule(pybind11::module_ m);
    }
}
#endif
