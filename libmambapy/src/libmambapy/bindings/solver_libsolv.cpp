// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>

#include "mamba/core/palette.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/solver/libsolv/parameters.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/solver/libsolv/solver.hpp"
#include "mamba/solver/libsolv/unsolvable.hpp"

#include "bindings.hpp"
#include "expected_caster.hpp"
#include "utils.hpp"

namespace mambapy
{
    void bind_submodule_solver_libsolv(pybind11::module_ m)
    {
        namespace py = pybind11;
        using namespace mamba;
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

        py::class_<UnSolvable>(m, "UnSolvable")
            .def("problems", &UnSolvable::problems)
            .def("problems_to_str", &UnSolvable::problems_to_str)
            .def("all_problems_to_str", &UnSolvable::all_problems_to_str)
            .def("problems_graph", &UnSolvable::problems_graph)
            .def("explain_problems", &UnSolvable::explain_problems);

        constexpr auto solver_flags_v2_migrator = [](Solver&, py::args, py::kwargs) {
            throw std::runtime_error("All flags need to be passed in the libmambapy.solver.Request.");
        };
        constexpr auto solver_job_v2_migrator = [](Solver&, py::args, py::kwargs) {
            throw std::runtime_error("All jobs need to be passed in the libmambapy.solver.Request.");
        };

        py::class_<Solver>(m, "Solver")  //
            .def(py::init())
            .def(
                "solve",
                [](Solver& self, MPool& pool, const solver::Request& request)
                { return self.solve(pool, request); }
            )
            .def("add_jobs", solver_job_v2_migrator)
            .def("add_global_job", solver_job_v2_migrator)
            .def("add_pin", solver_job_v2_migrator)
            .def("set_flags", solver_flags_v2_migrator)
            .def("set_libsolv_flags", solver_flags_v2_migrator)
            .def("set_postsolve_flags", solver_flags_v2_migrator)
            .def(
                "is_solved",
                [](Solver&, py::args, py::kwargs)
                {
                    // V2 migrator
                    throw std::runtime_error("Solve status is provided as an outcome to Solver.solve."
                    );
                }
            )
            .def(
                "try_solve",
                [](Solver&, py::args, py::kwargs)
                {
                    // V2 migrator
                    throw std::runtime_error("Use Solver.solve");
                }
            )
            .def(
                "must_solve",
                [](Solver&, py::args, py::kwargs)
                {
                    // V2 migrator
                    throw std::runtime_error("Use Solver.solve");
                }
            );
    }
}
