// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <pybind11/functional.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>

#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/solver/libsolv/solver.hpp"
#include "mamba/solver/libsolv/unsolvable.hpp"
#include "mamba/solver/parameters.hpp"
#include "mamba/solver/repo_info.hpp"

#include "bind_utils.hpp"
#include "bindings.hpp"
#include "expected_caster.hpp"
#include "path_caster.hpp"

namespace mambapy
{
    void bind_submodule_solver_libsolv(pybind11::module_ m)
    {
        namespace py = pybind11;
        using namespace mamba;
        using namespace mamba::solver;
        using namespace mamba::solver::libsolv;

        py::class_<Database>(m, "Database")
            .def(py::init<specs::ChannelResolveParams>(), py::arg("channel_params"))
            .def("set_logger", &Database::set_logger, py::call_guard<py::gil_scoped_acquire>())
            .def(
                "add_repo_from_repodata_json",
                &Database::add_repo_from_repodata_json,
                py::arg("path"),
                py::arg("url"),
                py::arg("channel_id"),
                py::arg("add_pip_as_python_dependency") = PipAsPythonDependency::No,
                py::arg("package_types") = PackageTypes::CondaOrElseTarBz2,
                py::arg("verify_packages") = VerifyPackages::No,
                py::arg("repodata_parser") = RepodataParser::Mamba
            )
            .def(
                "add_repo_from_native_serialization",
                &Database::add_repo_from_native_serialization,
                py::arg("path"),
                py::arg("expected"),
                py::arg("channel_id"),
                py::arg("add_pip_as_python_dependency") = PipAsPythonDependency::No
            )
            .def(
                "add_repo_from_packages",
                [](Database& db, py::iterable packages, std::string_view name, PipAsPythonDependency add)
                {
                    // TODO(C++20): No need to copy in a vector, simply transform the input range.
                    auto pkg_infos = std::vector<specs::PackageInfo>();
                    for (py::handle pkg : packages)
                    {
                        pkg_infos.push_back(pkg.cast<specs::PackageInfo>());
                    }
                    return db.add_repo_from_packages(pkg_infos, name, add);
                },
                py::arg("packages"),
                py::arg("name") = "",
                py::arg("add_pip_as_python_dependency") = PipAsPythonDependency::No
            )
            .def(
                "native_serialize_repo",
                &Database::native_serialize_repo,
                py::arg("repo"),
                py::arg("path"),
                py::arg("metadata")
            )
            .def("set_installed_repo", &Database::set_installed_repo, py::arg("repo"))
            .def("installed_repo", &Database::installed_repo)
            .def("set_repo_priority", &Database::set_repo_priority, py::arg("repo"), py::arg("priorities"))
            .def("remove_repo", &Database::remove_repo, py::arg("repo"))
            .def("repo_count", &Database::repo_count)
            .def("package_count", &Database::package_count)
            .def(
                "packages_in_repo",
                [](const Database& db, solver::RepoInfo repo)
                {
                    // TODO(C++20): When Database function are refactored to use range, take the
                    // opportunity here to make a Python iterator to avoid large alloc.
                    auto out = py::list();
                    db.for_each_package_in_repo(
                        repo,
                        [&](specs::PackageInfo&& pkg) { out.append(std::move(pkg)); }
                    );
                    return out;
                },
                py::arg("repo")
            )
            .def(
                "packages_matching",
                [](Database& db, const specs::MatchSpec& ms)
                {
                    // TODO(C++20): When Database function are refactored to use range, take the
                    // opportunity here to make a Python iterator to avoid large alloc.
                    auto out = py::list();
                    db.for_each_package_matching(
                        ms,
                        [&](specs::PackageInfo&& pkg) { out.append(std::move(pkg)); }
                    );
                    return out;
                },
                py::arg("spec")
            )
            .def(
                "packages_depending_on",
                [](Database& db, const specs::MatchSpec& ms)
                {
                    // TODO(C++20): When Database function are refactored to use range, take the
                    // opportunity here to make a Python iterator to avoid large alloc.
                    auto out = py::list();
                    db.for_each_package_depending_on(
                        ms,
                        [&](specs::PackageInfo&& pkg) { out.append(std::move(pkg)); }
                    );
                    return out;
                },
                py::arg("spec")
            );

        py::class_<UnSolvable>(m, "UnSolvable")
            .def("problems", &UnSolvable::problems, py::arg("database"))
            .def("problems_to_str", &UnSolvable::problems_to_str, py::arg("database"))
            .def("all_problems_to_str", &UnSolvable::all_problems_to_str, py::arg("database"))
            .def("problems_graph", &UnSolvable::problems_graph, py::arg("database"))
            .def(
                "explain_problems",
                &UnSolvable::explain_problems,
                py::arg("database"),
                py::arg("format")
            );

        constexpr auto solver_flags_v2_migrator = [](Solver&, py::args, py::kwargs)
        {
            throw std::runtime_error("All flags need to be passed in the libmambapy.solver.Request.");
        };
        constexpr auto solver_job_v2_migrator = [](Solver&, py::args, py::kwargs)
        { throw std::runtime_error("All jobs need to be passed in the libmambapy.solver.Request."); };

        py::class_<Solver>(m, "Solver")  //
            .def(py::init())
            .def(
                "solve",
                [](Solver& self, Database& db, const solver::Request& request)
                { return self.solve(db, request); }
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
