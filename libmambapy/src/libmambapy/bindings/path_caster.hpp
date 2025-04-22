// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <pybind11/pybind11.h>
#include <pybind11/stl/filesystem.h>

#include "mamba/fs/filesystem.hpp"

// The ODR warning occurs because pybind11's type_caster is defined in multiple translation units.
// This is unavoidable because:
//
// 1. The `type_caster` specialization must be defined in a header file to be available to all
// translation units
// 2. pybind11's own type_caster is defined in `cast.h` which is included in multiple places
// 3. We cannot make the specialization inline or move it to a source file without breaking
// pybind11's type system.
//
// Therefore, we silence the warning as it's a false positive in this case -
// the definitions are identical.

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wodr"
#endif

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

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
