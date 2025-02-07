// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <pybind11/pybind11.h>
#include <pybind11/stl/filesystem.h>

#include "mamba/fs/filesystem.hpp"

namespace PYBIND11_NAMESPACE
{
    namespace detail
    {
        template <>
        struct type_caster<mamba::fs::u8path> : path_caster<mamba::fs::u8path>
        {
        };
    }
}
