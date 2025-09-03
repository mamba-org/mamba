// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <ranges>

#include <pybind11/functional.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>

#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/parameters.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/solver/libsolv/solver.hpp"
#include "mamba/solver/libsolv/unsolvable.hpp"

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
        using namespace mamba::solver::libsolv;

        make_str_enum(
            py::enum_<RepodataParser>(m, "RepodataParser"),
            std::array{
                std::pair{ "Mamba", RepodataParser::Mamba },
                std::pair{ "Libsolv", RepodataParser::Libsolv },
            }
        );

        make_str_enum(
            py::enum_<MatchSpecParser>(m, "MatchSpecParser"),
            std::array{
                std::pair{ "Mixed", MatchSpecParser::Mixed },
                std::pair{ "Mamba", MatchSpecParser::Mamba },
                std::pair{ "Libsolv", MatchSpecParser::Libsolv },
            }
        );

        make_str_enum(
            py::enum_<PipAsPythonDependency>(m, "PipAsPythonDependency"),
            std::array{
                std::pair{ "No", PipAsPythonDependency::No },
                std::pair{ "Yes", PipAsPythonDependency::Yes },
            }
        );
        py::implicitly_convertible<py::bool_, PipAsPythonDependency>();

        make_str_enum(
            py::enum_<PackageTypes>(m, "PackageTypes"),
            std::array{
                std::pair{ "CondaOnly", PackageTypes::CondaOnly },
                std::pair{ "TarBz2Only", PackageTypes::TarBz2Only },
                std::pair{ "CondaAndTarBz2", PackageTypes::CondaAndTarBz2 },
                std::pair{ "CondaOrElseTarBz2", PackageTypes::CondaOrElseTarBz2 },
            }
        );

        make_str_enum(
            py::enum_<VerifyPackages>(m, "VerifyPackages"),
            std::array{
                std::pair{ "No", VerifyPackages::No },
                std::pair{ "Yes", VerifyPackages::Yes },
            }
        );
        py::implicitly_convertible<py::bool_, VerifyPackages>();

        make_str_enum(
            py::enum_<LogLevel>(m, "LogLevel"),
            std::array{
                std::pair{ "Debug", LogLevel::Debug },
                std::pair{ "Warning", LogLevel::Warning },
                std::pair{ "Error", LogLevel::Error },
                std::pair{ "Fatal", LogLevel::Fatal },
            }
        );

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
            .def_property_readonly("id", &RepoInfo::id)
            .def_property_readonly("name", &RepoInfo::name)
            .def_property_readonly("priority", &RepoInfo::priority)
            .def("package_count", &RepoInfo::package_count)
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def("__copy__", &copy<RepoInfo>)
            .def("__deepcopy__", &deepcopy<RepoInfo>, py::arg("memo"));

        py::class_<Database>(m, "Database")
            .def(
                py::init(
                    [](specs::ChannelResolveParams channel_params, MatchSpecParser matchspec_parser)
                    {
                        return Database(
                            channel_params,
                            Database::Settings{
                                matchspec_parser,
                            }
                        );
                    }
                ),
                py::arg("channel_params"),
                py::arg("matchspec_parser") = MatchSpecParser::Libsolv
            )
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
                [](Database& database,
                   py::iterable packages,
                   std::string_view name,
                   PipAsPythonDependency add)
                {
                    static constexpr auto cast = [](py::handle pkg)
                    { return pkg.cast<specs::PackageInfo>(); };

                    return database.add_repo_from_packages(
                        packages | std::ranges::views::transform(cast),
                        name,
                        add
                    );
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
                [](const Database& database, RepoInfo repo)
                {
                    // TODO(C++20): When Database function are refactored to use range, take the
                    // opportunity here to make a Python iterator to avoid large alloc.
                    auto out = py::list();
                    database.for_each_package_in_repo(
                        repo,
                        [&](specs::PackageInfo&& pkg) { out.append(std::move(pkg)); }
                    );
                    return out;
                },
                py::arg("repo")
            )
            .def(
                "packages_matching",
                [](Database& database, const specs::MatchSpec& ms)
                {
                    // TODO(C++20): When Database function are refactored to use range, take the
                    // opportunity here to make a Python iterator to avoid large alloc.
                    auto out = py::list();
                    database.for_each_package_matching(
                        ms,
                        [&](specs::PackageInfo&& pkg) { out.append(std::move(pkg)); }
                    );
                    return out;
                },
                py::arg("spec")
            )
            .def(
                "packages_depending_on",
                [](Database& database, const specs::MatchSpec& ms)
                {
                    // TODO(C++20): When Database function are refactored to use range, take the
                    // opportunity here to make a Python iterator to avoid large alloc.
                    auto out = py::list();
                    database.for_each_package_depending_on(
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

        py::class_<Solver>(m, "Solver")
            .def(py::init())
            .def(
                "solve",
                [](Solver& self, Database& database, const solver::Request& request, MatchSpecParser ms_parser
                ) { return self.solve(database, request, ms_parser); },
                py::arg("database"),
                py::arg("request"),
                py::arg("matchspec_parser") = MatchSpecParser::Mixed
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
