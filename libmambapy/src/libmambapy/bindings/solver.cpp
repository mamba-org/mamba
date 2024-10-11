// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "mamba/solver/problems_graph.hpp"
#include "mamba/solver/request.hpp"
#include "mamba/solver/solution.hpp"

#include "bind_utils.hpp"
#include "bindings.hpp"
#include "flat_set_caster.hpp"

namespace mamba::solver
{
    // Fix Pybind11 py::bind_vector<Request::job_list> has trouble detecting the absence
    // of comparison operators, so we tell it explicitly.
    auto operator==(const Request::Job&, const Request::Job&) -> bool = delete;
    auto operator!=(const Request::Job&, const Request::Job&) -> bool = delete;

    // Fix Pybind11 py::bind_vector<Solution::actions> has trouble detecting the absence
    // of comparison operators, so we tell it explicitly.
    auto operator==(const Solution::Action&, const Solution::Action&) -> bool = delete;
    auto operator!=(const Solution::Action&, const Solution::Action&) -> bool = delete;
}

PYBIND11_MAKE_OPAQUE(mamba::solver::Request::job_list);
PYBIND11_MAKE_OPAQUE(mamba::solver::Solution::action_list);

namespace mambapy
{
    void bind_submodule_solver(pybind11::module_ m)
    {
        namespace py = pybind11;
        using namespace mamba;
        using namespace mamba::solver;

        auto py_request = py::class_<Request>(m, "Request");

        py::class_<Request::Install>(py_request, "Install")
            .def(
                py::init(
                    [](specs::MatchSpec spec) -> Request::Install { return { std::move(spec) }; }
                ),
                py::arg("spec")
            )
            .def_readwrite("spec", &Request::Install::spec)
            .def("__copy__", &copy<Request::Install>)
            .def("__deepcopy__", &deepcopy<Request::Install>, py::arg("memo"));

        py::class_<Request::Remove>(py_request, "Remove")
            .def(
                py::init(
                    [](specs::MatchSpec spec, bool clean) -> Request::Remove
                    { return { std::move(spec), clean }; }
                ),
                py::arg("spec"),
                py::arg("clean_dependencies") = true
            )
            .def_readwrite("spec", &Request::Remove::spec)
            .def_readwrite("clean_dependencies", &Request::Remove::clean_dependencies)
            .def("__copy__", &copy<Request::Remove>)
            .def("__deepcopy__", &deepcopy<Request::Remove>, py::arg("memo"));

        py::class_<Request::Update>(py_request, "Update")
            .def(
                py::init(
                    [](specs::MatchSpec spec, bool clean) -> Request::Update
                    { return { std::move(spec), clean }; }
                ),
                py::arg("spec"),
                py::arg("clean_dependencies") = true
            )
            .def_readwrite("spec", &Request::Update::spec)
            .def_readwrite("clean_dependencies", &Request::Update::clean_dependencies)
            .def("__copy__", &copy<Request::Update>)
            .def("__deepcopy__", &deepcopy<Request::Update>, py::arg("memo"));

        py::class_<Request::UpdateAll>(py_request, "UpdateAll")
            .def(
                py::init([](bool clean) -> Request::UpdateAll { return { clean }; }),
                py::arg("clean_dependencies") = true
            )
            .def_readwrite("clean_dependencies", &Request::UpdateAll::clean_dependencies)
            .def("__copy__", &copy<Request::UpdateAll>)
            .def("__deepcopy__", &deepcopy<Request::UpdateAll>, py::arg("memo"));

        py::class_<Request::Keep>(py_request, "Keep")
            .def(
                py::init([](specs::MatchSpec spec) -> Request::Keep { return { std::move(spec) }; }),
                py::arg("spec")
            )
            .def_readwrite("spec", &Request::Keep::spec)
            .def("__copy__", &copy<Request::Keep>)
            .def("__deepcopy__", &deepcopy<Request::Keep>, py::arg("memo"));

        py::class_<Request::Freeze>(py_request, "Freeze")
            .def(
                py::init([](specs::MatchSpec spec) -> Request::Freeze { return { std::move(spec) }; }),
                py::arg("spec")
            )
            .def_readwrite("spec", &Request::Freeze::spec)
            .def("__copy__", &copy<Request::Freeze>)
            .def("__deepcopy__", &deepcopy<Request::Freeze>, py::arg("memo"));

        py::class_<Request::Pin>(py_request, "Pin")
            .def(
                py::init([](specs::MatchSpec spec) -> Request::Pin { return { std::move(spec) }; }),
                py::arg("spec")
            )
            .def_readwrite("spec", &Request::Pin::spec)
            .def("__copy__", &copy<Request::Pin>)
            .def("__deepcopy__", &deepcopy<Request::Pin>, py::arg("memo"));

        // Type made opaque at the top of this file
        py::bind_vector<Request::job_list>(py_request, "JobList");

        py::class_<Request::Flags>(py_request, "Flags")
            .def(
                py::init(
                    [](bool keep_dependencies,
                       bool keep_user_specs,
                       bool force_reinstall,
                       bool allow_downgrade,
                       bool allow_uninstall,
                       bool strict_repo_priority,
                       bool order_request) -> Request::Flags
                    {
                        return {
                            /* .keep_dependencies= */ keep_dependencies,
                            /* .keep_user_specs= */ keep_user_specs,
                            /* .force_reinstall= */ force_reinstall,
                            /* .allow_downgrade= */ allow_downgrade,
                            /* .allow_uninstall= */ allow_uninstall,
                            /* .strict_repo_priority= */ strict_repo_priority,
                            /* .order_request= */ order_request,
                        };
                    }
                ),
                py::arg("keep_dependencies") = true,
                py::arg("keep_user_specs") = true,
                py::arg("force_reinstall") = false,
                py::arg("allow_downgrade") = true,
                py::arg("allow_uninstall") = true,
                py::arg("strict_repo_priority") = true,
                py::arg("order_request") = true
            )
            .def_readwrite("keep_dependencies", &Request::Flags::keep_dependencies)
            .def_readwrite("keep_user_specs", &Request::Flags::keep_user_specs)
            .def_readwrite("force_reinstall", &Request::Flags::force_reinstall)
            .def_readwrite("allow_downgrade", &Request::Flags::allow_downgrade)
            .def_readwrite("allow_uninstall", &Request::Flags::allow_uninstall)
            .def_readwrite("strict_repo_priority", &Request::Flags::strict_repo_priority)
            .def_readwrite("order_request", &Request::Flags::order_request)
            .def("__copy__", &copy<Request::Flags>)
            .def("__deepcopy__", &deepcopy<Request::Flags>, py::arg("memo"));

        py_request
            .def(
                // Big copy unfortunately
                py::init(
                    [](Request::job_list jobs, Request::Flags flags) -> Request
                    { return { std::move(flags), std::move(jobs) }; }
                ),
                py::arg("jobs"),
                py::arg("flags") = Request::Flags()
            )
            .def(
                py::init(
                    [](py::iterable jobs, Request::Flags flags) -> Request
                    {
                        auto request = Request{ std::move(flags) };
                        request.jobs.reserve(py::len_hint(jobs));
                        for (py::handle itm : jobs)
                        {
                            request.jobs.push_back(py::cast<Request::Job>(itm));
                        }
                        return request;
                    }
                ),
                py::arg("jobs"),
                py::arg("flags") = Request::Flags()
            )
            .def_readwrite("flags", &Request::flags)
            .def_readwrite("jobs", &Request::jobs)
            .def("__copy__", &copy<Request>)
            .def("__deepcopy__", &deepcopy<Request>, py::arg("memo"));

        auto py_solution = py::class_<Solution>(m, "Solution");

        py::class_<Solution::Omit>(py_solution, "Omit")
            .def(
                py::init([](specs::PackageInfo pkg) -> Solution::Omit { return { std::move(pkg) }; }),
                py::arg("what")
            )
            .def_readwrite("what", &Solution::Omit::what)
            .def("__copy__", &copy<Solution::Omit>)
            .def("__deepcopy__", &deepcopy<Solution::Omit>, py::arg("memo"));

        py::class_<Solution::Upgrade>(py_solution, "Upgrade")
            .def(
                py::init(
                    [](specs::PackageInfo remove, specs::PackageInfo install) -> Solution::Upgrade
                    { return { std::move(remove), std::move(install) }; }
                ),
                py::arg("remove"),
                py::arg("install")
            )
            .def_readwrite("remove", &Solution::Upgrade::remove)
            .def_readwrite("install", &Solution::Upgrade::install)
            .def("__copy__", &copy<Solution::Upgrade>)
            .def("__deepcopy__", &deepcopy<Solution::Upgrade>, py::arg("memo"));

        py::class_<Solution::Downgrade>(py_solution, "Downgrade")
            .def(
                py::init(
                    [](specs::PackageInfo remove, specs::PackageInfo install) -> Solution::Downgrade
                    { return { std::move(remove), std::move(install) }; }
                ),
                py::arg("remove"),
                py::arg("install")
            )
            .def_readwrite("remove", &Solution::Downgrade::remove)
            .def_readwrite("install", &Solution::Downgrade::install)
            .def("__copy__", &copy<Solution::Downgrade>)
            .def("__deepcopy__", &deepcopy<Solution::Downgrade>, py::arg("memo"));

        py::class_<Solution::Change>(py_solution, "Change")
            .def(
                py::init(
                    [](specs::PackageInfo remove, specs::PackageInfo install) -> Solution::Change
                    { return { std::move(remove), std::move(install) }; }
                ),
                py::arg("remove"),
                py::arg("install")
            )
            .def_readwrite("remove", &Solution::Change::remove)
            .def_readwrite("install", &Solution::Change::install)
            .def("__copy__", &copy<Solution::Change>)
            .def("__deepcopy__", &deepcopy<Solution::Change>, py::arg("memo"));

        py::class_<Solution::Reinstall>(py_solution, "Reinstall")
            .def(
                py::init(
                    [](specs::PackageInfo pkg) -> Solution::Reinstall { return { std::move(pkg) }; }
                ),
                py::arg("what")
            )
            .def_readwrite("what", &Solution::Reinstall::what)
            .def("__copy__", &copy<Solution::Reinstall>)
            .def("__deepcopy__", &deepcopy<Solution::Reinstall>, py::arg("memo"));

        py::class_<Solution::Remove>(py_solution, "Remove")
            .def(
                py::init(
                    [](specs::PackageInfo remove) -> Solution::Remove
                    { return { std::move(remove) }; }
                ),
                py::arg("remove")
            )
            .def_readwrite("remove", &Solution::Remove::remove)
            .def("__copy__", &copy<Solution::Remove>)
            .def("__deepcopy__", &deepcopy<Solution::Remove>, py::arg("memo"));

        py::class_<Solution::Install>(py_solution, "Install")
            .def(
                py::init(
                    [](specs::PackageInfo install) -> Solution::Install
                    { return { std::move(install) }; }
                ),
                py::arg("install")
            )
            .def_readwrite("install", &Solution::Install::install)
            .def("__copy__", &copy<Solution::Install>)
            .def("__deepcopy__", &deepcopy<Solution::Install>, py::arg("memo"));

        // Type made opaque at the top of this file
        py::bind_vector<Solution::action_list>(py_solution, "ActionList");

        py_solution
            .def(
                // Big copy unfortunately
                py::init(
                    [](Solution::action_list actions) -> Solution { return { std::move(actions) }; }
                )
            )
            .def(py::init(
                [](py::iterable actions) -> Solution
                {
                    auto solution = Solution();
                    solution.actions.reserve(py::len_hint(actions));
                    for (py::handle act : actions)
                    {
                        solution.actions.push_back(py::cast<Solution::Action>(act));
                    }
                    return solution;
                }
            ))
            .def_readwrite("actions", &Solution::actions)
            .def(
                "to_install",
                [](const Solution& solution) -> std::vector<specs::PackageInfo>
                {
                    auto out = std::vector<specs::PackageInfo>{};
                    out.reserve(solution.actions.size());  // Upper bound
                    for_each_to_install(
                        solution.actions,
                        [&](const auto& pkg) { out.push_back(pkg); }
                    );
                    return out;
                }
            )
            .def(
                "to_remove",
                [](const Solution& solution) -> std::vector<specs::PackageInfo>
                {
                    auto out = std::vector<specs::PackageInfo>{};
                    out.reserve(solution.actions.size());  // Upper bound
                    for_each_to_remove(solution.actions, [&](const auto& pkg) { out.push_back(pkg); });
                    return out;
                }
            )
            .def(
                "to_omit",
                [](const Solution& solution) -> std::vector<specs::PackageInfo>
                {
                    auto out = std::vector<specs::PackageInfo>{};
                    out.reserve(solution.actions.size());  // Upper bound
                    for_each_to_omit(solution.actions, [&](const auto& pkg) { out.push_back(pkg); });
                    return out;
                }
            )
            .def("__copy__", &copy<Solution>)
            .def("__deepcopy__", &deepcopy<Solution>, py::arg("memo"));

        auto py_problems_graph = py::class_<ProblemsGraph>(m, "ProblemsGraph");

        py::class_<ProblemsGraph::RootNode>(py_problems_graph, "RootNode")  //
            .def(py::init<>());

        py::class_<ProblemsGraph::PackageNode, specs::PackageInfo>(py_problems_graph, "PackageNode")
            .def(py::init<specs::PackageInfo>());

        py::class_<ProblemsGraph::UnresolvedDependencyNode, specs::MatchSpec>(
            py_problems_graph,
            "UnresolvedDependencyNode"
        )
            .def(py::init<specs::MatchSpec>());

        py::class_<ProblemsGraph::ConstraintNode, specs::MatchSpec>(py_problems_graph, "ConstraintNode")
            .def(py::init<specs::MatchSpec>());

        py::class_<ProblemsGraph::conflicts_t>(py_problems_graph, "ConflictMap")
            .def(py::init([]() { return ProblemsGraph::conflicts_t(); }))
            .def("__len__", [](const ProblemsGraph::conflicts_t& self) { return self.size(); })
            .def("__bool__", [](const ProblemsGraph::conflicts_t& self) { return !self.empty(); })
            .def(
                "__iter__",
                [](const ProblemsGraph::conflicts_t& self)
                { return py::make_iterator(self.begin(), self.end()); },
                py::keep_alive<0, 1>()
            )
            .def("has_conflict", &ProblemsGraph::conflicts_t::has_conflict)
            .def("__contains__", &ProblemsGraph::conflicts_t::has_conflict)
            .def("conflicts", &ProblemsGraph::conflicts_t::conflicts)
            .def("in_conflict", &ProblemsGraph::conflicts_t::in_conflict)
            .def("clear", [](ProblemsGraph::conflicts_t& self) { return self.clear(); })
            .def("add", &ProblemsGraph::conflicts_t::add)
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def("__copy__", &copy<ProblemsGraph::conflicts_t>)
            .def("__deepcopy__", &deepcopy<ProblemsGraph::conflicts_t>, py::arg("memo"));


        py_problems_graph.def("root_node", &ProblemsGraph::root_node)
            .def("conflicts", &ProblemsGraph::conflicts)
            .def(
                "graph",
                [](const ProblemsGraph& self)
                {
                    const auto& g = self.graph();
                    return std::pair(g.nodes(), g.edges());
                }
            )
            .def_static("simplify_conflicts", &solver::simplify_conflicts);

        py::class_<ProblemsMessageFormat>(m, "ProblemsMessageFormat")
            .def(py::init<>())
            .def(
                py::init(
                    [](fmt::text_style unavailable,
                       fmt::text_style available,
                       std::array<std::string_view, 4> indents) -> ProblemsMessageFormat
                    {
                        return {
                            /* .unavailable= */ unavailable,
                            /* .available= */ available,
                            /* .indents= */ indents,
                        };
                    }
                ),
                py::arg("unavailable"),
                py::arg("available"),
                py::arg("indents")
            )
            .def_readwrite("unavailable", &ProblemsMessageFormat::unavailable)
            .def_readwrite("available", &ProblemsMessageFormat::available)
            .def_readwrite("indents", &ProblemsMessageFormat::indents)
            .def("__copy__", &copy<ProblemsMessageFormat>)
            .def("__deepcopy__", &deepcopy<ProblemsMessageFormat>, py::arg("memo"));

        auto py_compressed_problems_graph = py::class_<CompressedProblemsGraph>(
            m,
            "CompressedProblemsGraph"
        );

        auto bind_NamedList = [](auto&& pyclass)
        {
            using type = typename std::decay_t<decltype(pyclass)>::type;
            pyclass.def(py::init())
                .def("__len__", [](const type& self) { return self.size(); })
                .def("__bool__", [](const type& self) { return !self.empty(); })
                .def(
                    "__iter__",
                    [](const type& self) { return py::make_iterator(self.begin(), self.end()); },
                    py::keep_alive<0, 1>()
                )
                .def("clear", [](type& self) { return self.clear(); })
                .def("add", [](type& self, const typename type::value_type& v) { self.insert(v); })
                .def("name", &type::name)
                .def(
                    "versions_trunc",
                    &type::versions_trunc,
                    py::arg("sep") = "|",
                    py::arg("etc") = "...",
                    py::arg("threshold") = 5,
                    py::arg("remove_duplicates") = true
                )
                .def(
                    "build_strings_trunc",
                    &type::build_strings_trunc,
                    py::arg("sep") = "|",
                    py::arg("etc") = "...",
                    py::arg("threshold") = 5,
                    py::arg("remove_duplicates") = true
                )
                .def(
                    "versions_and_build_strings_trunc",
                    &type::versions_and_build_strings_trunc,
                    py::arg("sep") = "|",
                    py::arg("etc") = "...",
                    py::arg("threshold") = 5,
                    py::arg("remove_duplicates") = true
                );
            return pyclass;
        };

        py_compressed_problems_graph.def_property_readonly_static(
            "RootNode",
            [](py::handle) { return py::type::of<ProblemsGraph::RootNode>(); }
        );
        bind_NamedList(py::class_<CompressedProblemsGraph::PackageListNode>(
            py_compressed_problems_graph,
            "PackageListNode"
        ));
        bind_NamedList(py::class_<CompressedProblemsGraph::UnresolvedDependencyListNode>(
            py_compressed_problems_graph,
            "UnresolvedDependencyListNode"
        ));
        bind_NamedList(py::class_<CompressedProblemsGraph::ConstraintListNode>(
            py_compressed_problems_graph,
            "ConstraintListNode"
        ));
        bind_NamedList(
            py::class_<CompressedProblemsGraph::edge_t>(py_compressed_problems_graph, "DependencyList")
        );
        py_compressed_problems_graph.def_property_readonly_static(
            "ConflictMap",
            [](py::handle) { return py::type::of<ProblemsGraph::conflicts_t>(); }
        );

        py_compressed_problems_graph
            .def_static(
                "from_problems_graph",
                &CompressedProblemsGraph::from_problems_graph,
                py::arg("problems_graph"),
                py::arg("merge_criteria")
            )
            .def_static(
                "from_problems_graph",
                [](const ProblemsGraph& pbs)
                { return CompressedProblemsGraph::from_problems_graph(pbs); },
                py::arg("problems_graph")
            )
            .def("root_node", &CompressedProblemsGraph::root_node)
            .def("conflicts", &CompressedProblemsGraph::conflicts)
            .def(
                "graph",
                [](const CompressedProblemsGraph& self)
                {
                    const auto& g = self.graph();
                    return std::pair(g.nodes(), g.edges());
                }
            )
            .def("tree_message", &problem_tree_msg, py::arg("format") = ProblemsMessageFormat());
    }
}
