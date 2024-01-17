// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>

#include "mamba/solver/libsolv/parameters.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"

#include "bindings.hpp"
#include "utils.hpp"

namespace mambapy
{
    void bind_submodule_solver_libsolv(pybind11::module_ m)
    {
        namespace py = pybind11;
        using namespace mamba::solver::libsolv;

        py::class_<Priorities>(m, "Priorities")
            .def(
                py::init(
                    [](int priority, int subpriority) -> Priorities
                    {
                        return {
                            /* priority= */ priority,
                            /* subpriority= */ subpriority,
                        };
                    }
                ),
                py::arg("priority") = 0,
                py::arg("subpriority") = 0
            )
            .def_readwrite("priority", &Priorities::priority)
            .def_readwrite("subpriority", &Priorities::subpriority)
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def("__copy__", &copy<Priorities>)
            .def("__deepcopy__", &deepcopy<Priorities>, py::arg("memo"));

        py::class_<RepoInfo>(m, "RepoInfo")
            .def("id", &RepoInfo::id)
            .def("name", &RepoInfo::name)
            .def("priority", &RepoInfo::priority)
            .def("package_count", &RepoInfo::package_count)
            .def("__copy__", &copy<RepoInfo>)
            .def("__deepcopy__", &deepcopy<RepoInfo>, py::arg("memo"));
    }
}
