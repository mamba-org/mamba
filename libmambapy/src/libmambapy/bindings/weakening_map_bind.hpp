// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef LIBMAMBAPY_WEAKENING_MAP_BIND_HPP
#define LIBMAMBAPY_WEAKENING_MAP_BIND_HPP

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>

namespace mambapy
{
    template <typename Map, typename Py>
    auto bind_weakening_map(Py& m, const char* klass)
    {
        namespace py = pybind11;
        using key_type = typename Map::key_type;
        using mapped_type = typename Map::mapped_type;

        return py::bind_map<Map>(m, klass)
            .def(py::init(
                [](const py::dict& py_db)
                {
                    auto db = Map();
                    for (auto const& [name, auth] : py_db)
                    {
                        db.emplace(py::cast<key_type>(name), py::cast<mapped_type>(auth));
                    }
                    return db;
                }
            ))
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def("at_weaken", py::overload_cast<const std::string&>(&Map::at_weaken, py::const_))
            .def("contains_weaken", &Map::contains_weaken);
    }
}
#endif
