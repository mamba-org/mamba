// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef LIBMAMBAPY_BIND_UTILS_HPP
#define LIBMAMBAPY_BIND_UTILS_HPP

#include <memory>

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>

namespace mambapy
{
    template <typename Enum>
    auto enum_from_str(const pybind11::str& name)
    {
        auto pyenum = pybind11::type::of<Enum>();
        return pyenum.attr("__members__")[name].template cast<Enum>();
    }

    template <typename T>
    auto copy(const T& x) -> std::unique_ptr<T>
    {
        return std::make_unique<T>(x);
    }

    template <typename T>
    auto deepcopy(const T& x, const pybind11::dict& /* memo */) -> std::unique_ptr<T>
    {
        return std::make_unique<T>(x);
    }

    template <typename T>
    auto hash(const T& x) -> std::size_t
    {
        return std::hash<T>()(x);
    }
}
#endif
