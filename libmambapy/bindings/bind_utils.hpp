// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef LIBMAMBAPY_BIND_UTILS_HPP
#define LIBMAMBAPY_BIND_UTILS_HPP

#include <memory>
#include <utility>

#include <fmt/format.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>

namespace mambapy
{
    /**
     * Bind enum values and add a converting string constructor.
     *
     * This functions appeared during the migration from pybind 2 to 3.
     * Pybind 3 introduced py::native_enum, and made changes to previous py::enum.
     * At this point, libmambapy was relying on the fact that one could create an enum
     * from a string, using a constructor, and an implicit conversion to pass function enum
     * parameters as strings (Python catnip).
     * This function was added to avoid breaking that contract.
     * ``pybind11::enum_`` was kept because ``pybind11::native_enum`` does not support
     * to this day constructors needed for implicit conversion.
     *
     * Perhaps ``pybind11::native_enum`` will support string implicit conversion in the future,
     * otherwise the implicit conversion could be broken in a major release.
     */
    template <typename Enum, std::size_t N, typename PyEnum>
    auto make_str_enum(PyEnum&& pyenum, std::array<std::pair<const char*, Enum>, N> name_values)
    {
        for (const auto& [name, val] : name_values)
        {
            pyenum.value(name, val);
        }

        pyenum.def(
            pybind11::init(
                [name_values](std::string_view s)
                {
                    for (const auto& [name, val] : name_values)
                    {
                        if (name == s)
                        {
                            return val;
                        }
                    }
                    throw pybind11::key_error(fmt::format("No member named {}", s));
                }
            )
        );

        pybind11::implicitly_convertible<pybind11::str, Enum>();
        return std::forward<PyEnum>(pyenum);
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
