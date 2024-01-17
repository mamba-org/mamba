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

        py::enum_<RepodataParser>(m, "RepodataParser")
            .value("Mamba", RepodataParser::Mamba)
            .value("Libsolv", RepodataParser::Libsolv)
            .def(py::init(&enum_from_str<RepodataParser>));
        py::implicitly_convertible<py::str, RepodataParser>();

        py::enum_<PipAsPythonDependency>(m, "PipAsPythonDependency")
            .value("No", PipAsPythonDependency::No)
            .value("Yes", PipAsPythonDependency::Yes)
            .def(py::init([](bool val) { return static_cast<PipAsPythonDependency>(val); }));
        py::implicitly_convertible<py::bool_, PipAsPythonDependency>();

        py::class_<Priorities>(m, "Priorities")
            .def(
                py::init(
                    [](int priority, int subpriority) -> Priorities
                    {
                        return {
                            /* .priority= */ priority,
                            /* .subpriority= */ subpriority,
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

        py::class_<RepodataOrigin>(m, "RepodataOrigin")
            .def(
                py::init(
                    [](std::string_view url, std::string_view etag, std::string_view mod) -> RepodataOrigin
                    {
                        return {
                            /* .url= */ std::string(url),
                            /* .etag= */ std::string(etag),
                            /* .mod= */ std::string(mod),
                        };
                    }
                ),
                py::arg("url") = "",
                py::arg("etag") = "",
                py::arg("mod") = ""
            )
            .def_readwrite("url", &RepodataOrigin::url)
            .def_readwrite("etag", &RepodataOrigin::etag)
            .def_readwrite("mod", &RepodataOrigin::mod)
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def("__copy__", &copy<RepodataOrigin>)
            .def("__deepcopy__", &deepcopy<RepodataOrigin>, py::arg("memo"));

        py::class_<RepoInfo>(m, "RepoInfo")
            .def("id", &RepoInfo::id)
            .def("name", &RepoInfo::name)
            .def("priority", &RepoInfo::priority)
            .def("package_count", &RepoInfo::package_count)
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def("__copy__", &copy<RepoInfo>)
            .def("__deepcopy__", &deepcopy<RepoInfo>, py::arg("memo"));
    }
}
