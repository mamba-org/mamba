// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <pybind11/stl.h>

#include "mamba/util/flat_set.hpp"

#ifndef MAMBA_PY_SET_CASTER_HPP
#define MAMBA_PY_SET_CASTER_HPP

namespace PYBIND11_NAMESPACE
{
    namespace detail
    {
        template <typename Key, typename Compare, typename Alloc>
        struct type_caster<mamba::util::flat_set<Key, Compare, Alloc>>
            : set_caster<mamba::util::flat_set<Key, Compare, Alloc>, Key>
        {
        };
    }
}
#endif
