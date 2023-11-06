// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <stdexcept>

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <pybind11/functional.h>
#include <pybind11/iostream.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <solv/solver.h>

#include "mamba/api/clean.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/download_progress_bar.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/query.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/satisfiability_error.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/core/subdirdata.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/core/validate.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/specs/version.hpp"
#include "mamba/util/flat_set.hpp"
#include "mamba/util/string.hpp"

#include "flat_set_caster.hpp"

namespace py = pybind11;

namespace query
{
    enum RESULT_FORMAT
    {
        JSON = 0,
        TREE = 1,
        TABLE = 2,
        PRETTY = 3,
        RECURSIVETABLE = 4,
    };
}

void
deprecated(const char* message)
{
    const auto warnings = py::module_::import("warnings");
    const auto builtins = py::module_::import("builtins");
    warnings.attr("warn")(message, builtins.attr("DeprecationWarning"), py::arg("stacklevel") = 2);
}

template <typename PyClass>
auto
bind_NamedList(PyClass pyclass)
{
    using type = typename PyClass::type;
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
}

namespace mambapy
{
    class Singletons
    {
    public:

        mamba::MainExecutor& main_executor()
        {
            return m_main_executor;
        }
        mamba::Context& context()
        {
            return m_context;
        }
        mamba::Console& console()
        {
            return m_console;
        }
        mamba::ChannelContext& channel_context()
        {
            return init_once(p_channel_context, m_context);
        }

        mamba::Configuration& config()
        {
            return m_config;
        }

    private:

        template <class T, class D>
        T& init_once(std::unique_ptr<T, D>& ptr, mamba::Context& context)
        {
            static std::once_flag init_flag;
            std::call_once(init_flag, [&] { ptr = std::make_unique<T>(context); });
            if (!ptr)
            {
                throw mamba::mamba_error(
                    fmt::format(
                        "attempt to use {} singleton instance after destruction",
                        typeid(T).name()
                    ),
                    mamba::mamba_error_code::internal_failure
                );
            }
            return *ptr;
        }

        mamba::MainExecutor m_main_executor;
        mamba::Context m_context{ { /* .enable_logging_and_signal_handling = */ true } };
        mamba::Console m_console{ m_context };
        // ChannelContext needs to be lazy initialized, to enusre the Context has been initialized
        // before
        std::unique_ptr<mamba::ChannelContext> p_channel_context = nullptr;
        mamba::Configuration m_config{ m_context };
    };

    Singletons singletons;

    // MSubdirData objects are movable only, and they need to be moved into
    // a std::vector before we call MSudbirData::download. Since we cannot
    // replicate the move semantics in Python, we encapsulate the creation
    // and the storage of MSubdirData objects in this class, to avoid
    // potential dangling references in Python.
    class SubdirIndex
    {
    public:

        struct Entry
        {
            mamba::MSubdirData* p_subdirdata = nullptr;
            std::string m_platform = "";
            const mamba::Channel* p_channel = nullptr;
            std::string m_url = "";
        };

        using entry_list = std::vector<Entry>;
        using iterator = entry_list::const_iterator;

        void create(
            mamba::ChannelContext& channel_context,
            const mamba::Channel& channel,
            const std::string& platform,
            const std::string& full_url,
            mamba::MultiPackageCache& caches,
            const std::string& repodata_fn,
            const std::string& url
        )
        {
            using namespace mamba;
            m_subdirs.push_back(extract(
                MSubdirData::create(channel_context, channel, platform, full_url, caches, repodata_fn)
            ));
            m_entries.push_back({ nullptr, platform, &channel, url });
            for (size_t i = 0; i < m_subdirs.size(); ++i)
            {
                m_entries[i].p_subdirdata = &(m_subdirs[i]);
            }
        }

        bool download()
        {
            using namespace mamba;
            // TODO: expose SubdirDataMonitor to libmambapy and remove this
            //  logic
            Context& ctx = mambapy::singletons.context();
            expected_t<void> download_res;
            if (SubdirDataMonitor::can_monitor(ctx))
            {
                SubdirDataMonitor check_monitor({ true, true });
                SubdirDataMonitor index_monitor;
                download_res = MSubdirData::download_indexes(
                    m_subdirs,
                    ctx,
                    &check_monitor,
                    &index_monitor
                );
            }
            else
            {
                download_res = MSubdirData::download_indexes(m_subdirs, ctx);
            }
            return download_res.has_value();
        }

        std::size_t size() const
        {
            return m_entries.size();
        }

        const Entry& operator[](std::size_t index) const
        {
            return m_entries[index];
        }

        iterator begin() const
        {
            return m_entries.begin();
        }

        iterator end() const
        {
            return m_entries.end();
        }

    private:

        std::vector<mamba::MSubdirData> m_subdirs;
        entry_list m_entries;
    };
}

void
bind_submodule_impl(pybind11::module_ m)
{
    using namespace mamba;

    py::class_<specs::Version>(m, "Version")
        .def_static("parse", &specs::Version::parse)
        .def("__str__", &specs::Version::str);


    // declare earlier to avoid C++ types in docstrings
    auto pyChannel = py::class_<Channel, std::unique_ptr<Channel, py::nodelete>>(m, "Channel");
    auto pyPackageInfo = py::class_<PackageInfo>(m, "PackageInfo");
    auto pyPrefixData = py::class_<PrefixData>(m, "PrefixData");
    auto pySolver = py::class_<MSolver>(m, "Solver");

    // only used in a return type; does it belong in the module?
    auto pyRootRole = py::class_<validation::RootRole>(m, "RootRole");

    py::class_<fs::u8path>(m, "Path")
        .def(py::init<std::string>())
        .def("__str__", [](fs::u8path& self) -> std::string { return self.string(); })
        .def(
            "__repr__",
            [](fs::u8path& self) -> std::string
            { return fmt::format("fs::u8path[{}]", self.string()); }
        );
    py::implicitly_convertible<std::string, fs::u8path>();

    py::class_<mamba::LockFile>(m, "LockFile").def(py::init<fs::u8path>());

    py::register_exception<mamba_error>(m, "MambaNativeException");

    py::add_ostream_redirect(m, "ostream_redirect");

    py::class_<MatchSpec>(m, "MatchSpec")
        .def(py::init<>())
        .def(py::init<>(
            [](const std::string& name) {
                return MatchSpec{ name, mambapy::singletons.channel_context() };
            }
        ))
        .def("conda_build_form", &MatchSpec::conda_build_form);

    py::class_<MPool>(m, "Pool")
        .def(py::init<>([] { return MPool{ mambapy::singletons.channel_context() }; }))
        .def("set_debuglevel", &MPool::set_debuglevel)
        .def("create_whatprovides", &MPool::create_whatprovides)
        .def("select_solvables", &MPool::select_solvables, py::arg("id"), py::arg("sorted") = false)
        .def("matchspec2id", &MPool::matchspec2id, py::arg("ms"))
        .def(
            "matchspec2id",
            [](MPool& self, std::string_view ms) {
                return self.matchspec2id({ ms, mambapy::singletons.channel_context() });
            },
            py::arg("ms")
        )
        .def("id2pkginfo", &MPool::id2pkginfo, py::arg("id"));

    py::class_<MultiPackageCache>(m, "MultiPackageCache")
        .def(py::init<>(
            [](const std::vector<fs::u8path>& pkgs_dirs)
            {
                return MultiPackageCache{
                    pkgs_dirs,
                    mambapy::singletons.context().validation_params,
                };
            }
        ))
        .def("get_tarball_path", &MultiPackageCache::get_tarball_path)
        .def_property_readonly("first_writable_path", &MultiPackageCache::first_writable_path);

    py::class_<MRepo::PyExtraPkgInfo>(m, "ExtraPkgInfo")
        .def(py::init<>())
        .def_readwrite("noarch", &MRepo::PyExtraPkgInfo::noarch)
        .def_readwrite("repo_url", &MRepo::PyExtraPkgInfo::repo_url);

    py::class_<MRepo>(m, "Repo")
        .def(py::init(
            [](MPool& pool, const std::string& name, const std::string& filename, const std::string& url
            ) { return MRepo(pool, name, filename, RepoMetadata{ /* .url=*/url }); }
        ))
        .def(py::init<MPool&, const PrefixData&>())
        .def("add_extra_pkg_info", &MRepo::py_add_extra_pkg_info)
        .def("set_installed", &MRepo::set_installed)
        .def("set_priority", &MRepo::set_priority)
        .def("name", &MRepo::py_name)
        .def("priority", &MRepo::py_priority)
        .def("size", &MRepo::py_size)
        .def("clear", &MRepo::py_clear);

    py::class_<MTransaction>(m, "Transaction")
        .def(py::init<>(
            [](MSolver& solver, MultiPackageCache& mpc)
            {
                deprecated("Use Transaction(Pool, Solver, MultiPackageCache) instead");
                return std::make_unique<MTransaction>(solver.pool(), solver, mpc);
            }
        ))
        .def(py::init<MPool&, MSolver&, MultiPackageCache&>())
        .def("to_conda", &MTransaction::to_conda)
        .def("log_json", &MTransaction::log_json)
        .def("print", &MTransaction::print)
        .def("fetch_extract_packages", &MTransaction::fetch_extract_packages)
        .def("prompt", &MTransaction::prompt)
        .def("find_python_version", &MTransaction::py_find_python_version)
        .def("execute", &MTransaction::execute);

    py::class_<MSolverProblem>(m, "SolverProblem")
        .def_readwrite("type", &MSolverProblem::type)
        .def_readwrite("source_id", &MSolverProblem::source_id)
        .def_readwrite("target_id", &MSolverProblem::target_id)
        .def_readwrite("dep_id", &MSolverProblem::dep_id)
        .def_readwrite("source", &MSolverProblem::source)
        .def_readwrite("target", &MSolverProblem::target)
        .def_readwrite("dep", &MSolverProblem::dep)
        .def_readwrite("description", &MSolverProblem::description)
        .def("__str__", [](const MSolverProblem& self) { return self.description; });

    pySolver.def(py::init<MPool&, std::vector<std::pair<int, int>>>(), py::keep_alive<1, 2>())
        .def("add_jobs", &MSolver::add_jobs)
        .def("add_global_job", &MSolver::add_global_job)
        .def("add_constraint", &MSolver::add_constraint)
        .def("add_pin", &MSolver::add_pin)
        .def("set_flags", &MSolver::py_set_libsolv_flags)
        .def("set_postsolve_flags", &MSolver::py_set_postsolve_flags)
        .def("is_solved", &MSolver::is_solved)
        .def("problems_to_str", &MSolver::problems_to_str)
        .def("all_problems_to_str", &MSolver::all_problems_to_str)
        .def("explain_problems", py::overload_cast<>(&MSolver::explain_problems, py::const_))
        .def("all_problems_structured", &MSolver::all_problems_structured)
        .def(
            "solve",
            [](MSolver& self)
            {
                // TODO figure out a better interface
                return self.try_solve();
            }
        )
        .def("try_solve", &MSolver::try_solve)
        .def("must_solve", &MSolver::must_solve);

    using PbGraph = ProblemsGraph;
    auto pyPbGraph = py::class_<PbGraph>(m, "ProblemsGraph");

    py::class_<PbGraph::RootNode>(pyPbGraph, "RootNode").def(py::init<>());
    py::class_<PbGraph::PackageNode, PackageInfo>(pyPbGraph, "PackageNode");
    py::class_<PbGraph::UnresolvedDependencyNode, MatchSpec>(pyPbGraph, "UnresolvedDependencyNode");
    py::class_<PbGraph::ConstraintNode, MatchSpec>(pyPbGraph, "ConstraintNode");

    py::class_<PbGraph::conflicts_t>(pyPbGraph, "ConflictMap")
        .def(py::init([]() { return PbGraph::conflicts_t(); }))
        .def("__len__", [](const PbGraph::conflicts_t& self) { return self.size(); })
        .def("__bool__", [](const PbGraph::conflicts_t& self) { return !self.empty(); })
        .def(
            "__iter__",
            [](const PbGraph::conflicts_t& self)
            { return py::make_iterator(self.begin(), self.end()); },
            py::keep_alive<0, 1>()
        )
        .def("has_conflict", &PbGraph::conflicts_t::has_conflict)
        .def("__contains__", &PbGraph::conflicts_t::has_conflict)
        .def("conflicts", &PbGraph::conflicts_t::conflicts)
        .def("in_conflict", &PbGraph::conflicts_t::in_conflict)
        .def("clear", [](PbGraph::conflicts_t& self) { return self.clear(); })
        .def("add", &PbGraph::conflicts_t::add);

    pyPbGraph
        .def_static(
            "from_solver",
            [](const MSolver& solver, const MPool& /* pool */)
            {
                deprecated("Use Solver.problems_graph() instead");
                return solver.problems_graph();
            }
        )
        .def("root_node", &PbGraph::root_node)
        .def("conflicts", &PbGraph::conflicts)
        .def(
            "graph",
            [](const PbGraph& self)
            {
                auto const& g = self.graph();
                return std::pair(g.nodes(), g.edges());
            }
        );

    m.def("simplify_conflicts", &simplify_conflicts);

    using CpPbGraph = CompressedProblemsGraph;
    auto pyCpPbGraph = py::class_<CpPbGraph>(m, "CompressedProblemsGraph");

    pyCpPbGraph.def_property_readonly_static(
        "RootNode",
        [](py::handle) { return py::type::of<PbGraph::RootNode>(); }
    );
    bind_NamedList(py::class_<CpPbGraph::PackageListNode>(pyCpPbGraph, "PackageListNode"));
    bind_NamedList(
        py::class_<CpPbGraph::UnresolvedDependencyListNode>(pyCpPbGraph, "UnresolvedDependencyListNode")
    );
    bind_NamedList(py::class_<CpPbGraph::ConstraintListNode>(pyCpPbGraph, "ConstraintListNode"));
    bind_NamedList(py::class_<CpPbGraph::edge_t>(pyCpPbGraph, "DependencyList"));
    pyCpPbGraph.def_property_readonly_static(
        "ConflictMap",
        [](py::handle) { return py::type::of<PbGraph::conflicts_t>(); }
    );

    pyCpPbGraph.def_static("from_problems_graph", &CpPbGraph::from_problems_graph)
        .def_static(
            "from_problems_graph",
            [](const PbGraph& pbs) { return CpPbGraph::from_problems_graph(pbs); }
        )
        .def("root_node", &CpPbGraph::root_node)
        .def("conflicts", &CpPbGraph::conflicts)
        .def(
            "graph",
            [](const CpPbGraph& self)
            {
                auto const& g = self.graph();
                return std::pair(g.nodes(), g.edges());
            }
        )
        .def("tree_message", [](const CpPbGraph& self) { return problem_tree_msg(self); });

    py::class_<History>(m, "History")
        .def(py::init(
            [](const fs::u8path& path) {
                return History{ path, mambapy::singletons.channel_context() };
            }
        ))
        .def("get_requested_specs_map", &History::get_requested_specs_map);

    /*py::class_<Query>(m, "Query")
        .def(py::init<MPool&>())
        .def("find", &Query::find)
        .def("whoneeds", &Query::whoneeds)
        .def("depends", &Query::depends)
    ;*/

    py::enum_<query::RESULT_FORMAT>(m, "QueryFormat")
        .value("JSON", query::RESULT_FORMAT::JSON)
        .value("TREE", query::RESULT_FORMAT::TREE)
        .value("TABLE", query::RESULT_FORMAT::TABLE)
        .value("PRETTY", query::RESULT_FORMAT::PRETTY)
        .value("RECURSIVETABLE", query::RESULT_FORMAT::RECURSIVETABLE);

    auto queries_find = [](const Query& q,
                           const std::vector<std::string>& queries,
                           const query::RESULT_FORMAT format) -> std::string
    {
        query_result res = q.find(queries);
        std::stringstream res_stream;
        switch (format)
        {
            case query::JSON:
                res_stream << res.groupby("name").json().dump(4);
                break;
            case query::TREE:
            case query::TABLE:
            case query::RECURSIVETABLE:
                res.groupby("name").table(res_stream);
                break;
            case query::PRETTY:
                res.groupby("name").pretty(res_stream, mambapy::singletons.context().output_params);
        }
        if (res.empty() && format != query::JSON)
        {
            res_stream << fmt::format("{}", fmt::join(queries, " "))
                       << " may not be installed. Try specifying a channel with '-c,--channel' option\n";
        }
        return res_stream.str();
    };

    py::class_<Query>(m, "Query")
        .def(py::init<MPool&>())
        .def(
            "find",
            [queries_find](const Query& q, const std::string& query, const query::RESULT_FORMAT format)
                -> std::string { return queries_find(q, { query }, format); }
        )
        .def("find", queries_find)
        .def(
            "whoneeds",
            [](const Query& q, const std::string& query, const query::RESULT_FORMAT format) -> std::string
            {
                // QueryResult res = q.whoneeds(query, tree);
                std::stringstream res_stream;
                query_result res = q.whoneeds(query, (format == query::TREE));
                switch (format)
                {
                    case query::TREE:
                    case query::PRETTY:
                        res.tree(res_stream, mambapy::singletons.context().graphics_params);
                        break;
                    case query::JSON:
                        res_stream << res.json().dump(4);
                        break;
                    case query::TABLE:
                    case query::RECURSIVETABLE:
                        res.table(
                            res_stream,
                            { "Name",
                              "Version",
                              "Build",
                              printers::alignmentMarker(printers::alignment::left),
                              printers::alignmentMarker(printers::alignment::right),
                              util::concat("Depends:", query),
                              "Channel",
                              "Subdir" }
                        );
                }
                if (res.empty() && format != query::JSON)
                {
                    res_stream << query
                               << " may not be installed. Try giving a channel with '-c,--channel' option for remote repoquery\n";
                }
                return res_stream.str();
            }
        )
        .def(
            "depends",
            [](const Query& q, const std::string& query, const query::RESULT_FORMAT format) -> std::string
            {
                query_result res = q.depends(
                    query,
                    (format == query::TREE || format == query::RECURSIVETABLE)
                );
                std::stringstream res_stream;
                switch (format)
                {
                    case query::TREE:
                    case query::PRETTY:
                        res.tree(res_stream, mambapy::singletons.context().graphics_params);
                        break;
                    case query::JSON:
                        res_stream << res.json().dump(4);
                        break;
                    case query::TABLE:
                    case query::RECURSIVETABLE:
                        // res.table(res_stream, {"Name", "Version", "Build", concat("Depends:",
                        // query), "Channel"});
                        res.table(res_stream);
                }
                if (res.empty() && format != query::JSON)
                {
                    res_stream << query
                               << " may not be installed. Try giving a channel with '-c,--channel' option for remote repoquery\n";
                }
                return res_stream.str();
            }
        );

    py::class_<MSubdirData>(m, "SubdirData")
        .def(
            "create_repo",
            [](MSubdirData& subdir, MPool& pool) -> MRepo
            { return extract(subdir.create_repo(pool)); }
        )
        .def("loaded", &MSubdirData::is_loaded)
        .def(
            "cache_path",
            [](const MSubdirData& self) -> std::string { return extract(self.cache_path()); }
        );

    using mambapy::SubdirIndex;
    using SubdirIndexEntry = SubdirIndex::Entry;

    py::class_<SubdirIndexEntry>(m, "SubdirIndexEntry")
        .def(py::init<>())
        .def_readonly("subdir", &SubdirIndexEntry::p_subdirdata, py::return_value_policy::reference)
        .def_readonly("platform", &SubdirIndexEntry::m_platform)
        .def_readonly("channel", &SubdirIndexEntry::p_channel, py::return_value_policy::reference)
        .def_readonly("url", &SubdirIndexEntry::m_url);

    py::class_<SubdirIndex>(m, "SubdirIndex")
        .def(py::init<>())
        .def(
            "create",
            [](SubdirIndex& self,
               const Channel& channel,
               const std::string& platform,
               const std::string& full_url,
               MultiPackageCache& caches,
               const std::string& repodata_fn,
               const std::string& url)
            {
                self.create(
                    mambapy::singletons.channel_context(),
                    channel,
                    platform,
                    full_url,
                    caches,
                    repodata_fn,
                    url
                );
            }
        )
        .def("download", &SubdirIndex::download)
        .def("__len__", &SubdirIndex::size)
        .def("__getitem__", &SubdirIndex::operator[])
        .def(
            "__iter__",
            [](SubdirIndex& self) { return py::make_iterator(self.begin(), self.end()); },
            py::keep_alive<0, 1>()
        );

    m.def("cache_fn_url", &cache_fn_url);
    m.def("create_cache_dir", &create_cache_dir);

    py::enum_<ChannelPriority>(m, "ChannelPriority")
        .value("Flexible", ChannelPriority::Flexible)
        .value("Strict", ChannelPriority::Strict)
        .value("Disabled", ChannelPriority::Disabled);

    py::enum_<mamba::log_level>(m, "LogLevel")
        .value("TRACE", mamba::log_level::trace)
        .value("DEBUG", mamba::log_level::debug)
        .value("INFO", mamba::log_level::info)
        .value("WARNING", mamba::log_level::warn)
        .value("ERROR", mamba::log_level::err)
        .value("CRITICAL", mamba::log_level::critical)
        .value("OFF", mamba::log_level::off);

    py::class_<Context, std::unique_ptr<Context, py::nodelete>> ctx(m, "Context");
    ctx.def(py::init(
                [] { return std::unique_ptr<Context, py::nodelete>(&mambapy::singletons.context()); }
            ))
        .def_readwrite("offline", &Context::offline)
        .def_readwrite("local_repodata_ttl", &Context::local_repodata_ttl)
        .def_readwrite("use_index_cache", &Context::use_index_cache)
        .def_readwrite("always_yes", &Context::always_yes)
        .def_readwrite("dry_run", &Context::dry_run)
        .def_readwrite("download_only", &Context::download_only)
        .def_readwrite("add_pip_as_python_dependency", &Context::add_pip_as_python_dependency)
        .def_readwrite("envs_dirs", &Context::envs_dirs)
        .def_readwrite("pkgs_dirs", &Context::pkgs_dirs)
        .def_readwrite("platform", &Context::platform)
        .def_readwrite("channels", &Context::channels)
        .def_readwrite("custom_channels", &Context::custom_channels)
        .def_readwrite("custom_multichannels", &Context::custom_multichannels)
        .def_readwrite("default_channels", &Context::default_channels)
        .def_readwrite("channel_alias", &Context::channel_alias)
        .def_readwrite("use_only_tar_bz2", &Context::use_only_tar_bz2)
        .def_readwrite("channel_priority", &Context::channel_priority)
        .def_property(
            "experimental_sat_error_message",
            [](const Context&)
            {
                deprecated("The new error messages are always enabled.");
                return true;
            },
            [](const Context&, bool)
            {
                deprecated("Setting ``Context.experimental_sat_error_message`` has no effect."
                           " The new error messages are always enabled.");
            }
        )
        .def_property(
            "use_lockfiles",
            [](Context& context)
            {
                context.use_lockfiles = is_file_locking_allowed();
                return context.use_lockfiles;
            },
            [](Context& context, bool allow)
            {
                allow_file_locking(allow);
                context.use_lockfiles = allow;
            }
        )
        .def("set_verbosity", &Context::set_verbosity)
        .def("set_log_level", &Context::set_log_level);

    py::class_<Context::RemoteFetchParams>(ctx, "RemoteFetchParams")
        .def(py::init<>())
        .def_readwrite("ssl_verify", &Context::RemoteFetchParams::ssl_verify)
        .def_readwrite("max_retries", &Context::RemoteFetchParams::max_retries)
        .def_readwrite("retry_timeout", &Context::RemoteFetchParams::retry_timeout)
        .def_readwrite("retry_backoff", &Context::RemoteFetchParams::retry_backoff)
        .def_readwrite("user_agent", &Context::RemoteFetchParams::user_agent)
        // .def_readwrite("read_timeout_secs", &Context::RemoteFetchParams::read_timeout_secs)
        .def_readwrite("proxy_servers", &Context::RemoteFetchParams::proxy_servers)
        .def_readwrite("connect_timeout_secs", &Context::RemoteFetchParams::connect_timeout_secs);

    py::class_<Context::OutputParams>(ctx, "OutputParams")
        .def(py::init<>())
        .def_readwrite("verbosity", &Context::OutputParams::verbosity)
        .def_readwrite("json", &Context::OutputParams::json)
        .def_readwrite("quiet", &Context::OutputParams::quiet);

    py::class_<Context::ThreadsParams>(ctx, "ThreadsParams")
        .def(py::init<>())
        .def_readwrite("download_threads", &Context::ThreadsParams::download_threads)
        .def_readwrite("extract_threads", &Context::ThreadsParams::extract_threads);

    py::class_<Context::PrefixParams>(ctx, "PrefixParams")
        .def(py::init<>())
        .def_readwrite("target_prefix", &Context::PrefixParams::target_prefix)
        .def_readwrite("conda_prefix", &Context::PrefixParams::conda_prefix)
        .def_readwrite("root_prefix", &Context::PrefixParams::root_prefix);

    ctx.def_readwrite("remote_fetch_params", &Context::remote_fetch_params)
        .def_readwrite("output_params", &Context::output_params)
        .def_readwrite("threads_params", &Context::threads_params)
        .def_readwrite("prefix_params", &Context::prefix_params);

    // TODO: uncomment these parameters once they are made available to Python api.
    // py::class_<ValidationOptions>(ctx, "ValidationOptions")
    //     .def_readwrite("safety_checks", &ValidationOptions::safety_checks)
    //     .def_readwrite("extra_safety_checks", &ValidationOptions::extra_safety_checks)
    //     .def_readwrite("verify_artifacts", &ValidationOptions::verify_artifacts);

    // ctx.def_readwrite("validation_params", &Context::validation_params);

    ////////////////////////////////////////////
    //    Support the old deprecated API     ///
    ////////////////////////////////////////////
    // RemoteFetchParams
    ctx.def_property(
           "ssl_verify",
           [](const Context& self)
           {
               deprecated("Use `remote_fetch_params.ssl_verify` instead.");
               return self.remote_fetch_params.ssl_verify;
           },
           [](Context& self, std::string sv)
           {
               deprecated("Use `remote_fetch_params.ssl_verify` instead.");
               self.remote_fetch_params.ssl_verify = sv;
           }
    )
        .def_property(
            "max_retries",
            [](const Context& self)
            {
                deprecated("Use `remote_fetch_params.max_retries` instead.");
                return self.remote_fetch_params.max_retries;
            },
            [](Context& self, int mr)
            {
                deprecated("Use `remote_fetch_params.max_retries` instead.");
                self.remote_fetch_params.max_retries = mr;
            }
        )
        .def_property(
            "retry_timeout",
            [](const Context& self)
            {
                deprecated("Use `remote_fetch_params.retry_timeout` instead.");
                return self.remote_fetch_params.retry_timeout;
            },
            [](Context& self, int rt)
            {
                deprecated("Use `remote_fetch_params.retry_timeout` instead.");
                self.remote_fetch_params.retry_timeout = rt;
            }
        )
        .def_property(
            "retry_backoff",
            [](const Context& self)
            {
                deprecated("Use `remote_fetch_params.retry_backoff` instead.");
                return self.remote_fetch_params.retry_backoff;
            },
            [](Context& self, int rb)
            {
                deprecated("Use `remote_fetch_params.retry_backoff` instead.");
                self.remote_fetch_params.retry_backoff = rb;
            }
        )
        .def_property(
            "user_agent",
            [](const Context& self)
            {
                deprecated("Use `remote_fetch_params.user_agent` instead.");
                return self.remote_fetch_params.user_agent;
            },
            [](Context& self, std::string ua)
            {
                deprecated("Use `remote_fetch_params.user_agent` instead.");
                self.remote_fetch_params.user_agent = ua;
            }
        )
        .def_property(
            "connect_timeout_secs",
            [](const Context& self)
            {
                deprecated("Use `remote_fetch_params.connect_timeout_secs` instead.");
                return self.remote_fetch_params.connect_timeout_secs;
            },
            [](Context& self, double cts)
            {
                deprecated("Use `remote_fetch_params.connect_timeout_secs` instead.");
                self.remote_fetch_params.connect_timeout_secs = cts;
            }
        )
        .def_property(
            "proxy_servers",
            [](const Context& self)
            {
                deprecated("Use `remote_fetch_params.proxy_servers` instead.");
                return self.remote_fetch_params.proxy_servers;
            },
            [](Context& self, const std::map<std::string, std::string>& proxies)
            {
                deprecated("Use `remote_fetch_params.proxy_servers` instead.");
                self.remote_fetch_params.proxy_servers = proxies;
            }
        );

    // OutputParams
    ctx.def_property(
           "verbosity",
           [](const Context& self)
           {
               deprecated("Use `output_params.verbosity` instead.");
               return self.output_params.verbosity;
           },
           [](Context& self, int v)
           {
               deprecated("Use `output_params.verbosity` instead.");
               self.output_params.verbosity = v;
           }
    )
        .def_property(
            "json",
            [](const Context& self)
            {
                deprecated("Use `output_params.json` instead.");
                return self.output_params.json;
            },
            [](Context& self, bool j)
            {
                deprecated("Use `output_params.json` instead.");
                self.output_params.json = j;
            }
        )
        .def_property(
            "quiet",
            [](const Context& self)
            {
                deprecated("Use `output_params.quiet` instead.");
                return self.output_params.quiet;
            },
            [](Context& self, bool q)
            {
                deprecated("Use `output_params.quiet` instead.");
                self.output_params.quiet = q;
            }
        );

    // ThreadsParams
    ctx.def_property(
           "download_threads",
           [](const Context& self)
           {
               deprecated("Use `threads_params.download_threads` instead.");
               return self.threads_params.download_threads;
           },
           [](Context& self, std::size_t dt)
           {
               deprecated("Use `threads_params.download_threads` instead.");
               self.threads_params.download_threads = dt;
           }
    )
        .def_property(
            "extract_threads",
            [](const Context& self)
            {
                deprecated("Use `threads_params.extract_threads` instead.");
                return self.threads_params.extract_threads;
            },
            [](Context& self, int et)
            {
                deprecated("Use `threads_params.extract_threads` instead.");
                self.threads_params.extract_threads = et;
            }
        );

    // PrefixParams
    ctx.def_property(
           "target_prefix",
           [](const Context& self)
           {
               deprecated("Use `prefix_params.target_prefix` instead.");
               return self.prefix_params.target_prefix;
           },
           [](Context& self, fs::u8path tp)
           {
               deprecated("Use `prefix_params.target_prefix` instead.");
               self.prefix_params.target_prefix = tp;
           }
    )
        .def_property(
            "conda_prefix",
            [](const Context& self)
            {
                deprecated("Use `prefix_params.conda_prefix` instead.");
                return self.prefix_params.conda_prefix;
            },
            [](Context& self, fs::u8path cp)
            {
                deprecated("Use `prefix_params.conda_prefix` instead.");
                self.prefix_params.conda_prefix = cp;
            }
        )
        .def_property(
            "root_prefix",
            [](const Context& self)
            {
                deprecated("Use `prefix_params.root_prefix` instead.");
                return self.prefix_params.root_prefix;
            },
            [](Context& self, fs::u8path rp)
            {
                deprecated("Use `prefix_params.root_prefix` instead.");
                self.prefix_params.root_prefix = rp;
            }
        );

    ////////////////////////////////////////////

    pyPrefixData
        .def(py::init(
            [](const fs::u8path& prefix_path) -> PrefixData
            {
                auto sres = PrefixData::create(prefix_path, mambapy::singletons.channel_context());
                if (sres.has_value())
                {
                    return std::move(sres.value());
                }
                else
                {
                    throw sres.error();
                }
            }
        ))
        .def_property_readonly("package_records", &PrefixData::records)
        .def("add_packages", &PrefixData::add_packages);

    pyPackageInfo  //
        .def(py::init<const std::string&>(), py::arg("name"))
        .def(
            py::init<const std::string&, const std::string&, const std::string&, std::size_t>(),
            py::arg("name"),
            py::arg("version"),
            py::arg("build_string"),
            py::arg("build_number")
        )
        .def_readwrite("name", &PackageInfo::name)
        .def_readwrite("version", &PackageInfo::version)
        .def_readwrite("build_string", &PackageInfo::build_string)
        .def_readwrite("build_number", &PackageInfo::build_number)
        .def_readwrite("noarch", &PackageInfo::noarch)
        .def_readwrite("channel", &PackageInfo::channel)
        .def_readwrite("url", &PackageInfo::url)
        .def_readwrite("subdir", &PackageInfo::subdir)
        .def_readwrite("fn", &PackageInfo::fn)
        .def_readwrite("license", &PackageInfo::license)
        .def_readwrite("size", &PackageInfo::size)
        .def_readwrite("timestamp", &PackageInfo::timestamp)
        .def_readwrite("md5", &PackageInfo::md5)
        .def_readwrite("sha256", &PackageInfo::sha256)
        .def_property(
            "track_features",
            [](const PackageInfo& self)
            {
                static_assert(LIBMAMBA_VERSION_MAJOR == 1, "Version 1 compatibility.");
                return fmt::format("{}", fmt::join(self.track_features, ","));
            },
            [](PackageInfo& self, std::string_view val)
            { self.track_features = util::split(val, ","); }
        )
        .def_readwrite("depends", &PackageInfo::depends)
        .def_readwrite("constrains", &PackageInfo::constrains)
        .def_readwrite("signatures", &PackageInfo::signatures)
        .def_readwrite("defaulted_keys", &PackageInfo::defaulted_keys);

    // Content trust - Package signature and verification
    m.def("generate_ed25519_keypair", &validation::generate_ed25519_keypair_hex);
    m.def(
        "sign",
        [](const std::string& data, const std::string& sk)
        {
            std::string signature;
            if (!validation::sign(data, sk, signature))
            {
                throw std::runtime_error("Signing failed");
            }
            return signature;
        },
        py::arg("data"),
        py::arg("secret_key")
    );

    py::class_<validation::Key>(m, "Key")
        .def_readwrite("keytype", &validation::Key::keytype)
        .def_readwrite("scheme", &validation::Key::scheme)
        .def_readwrite("keyval", &validation::Key::keyval)
        .def_property_readonly(
            "json_str",
            [](const validation::Key& key)
            {
                nlohmann::json j;
                validation::to_json(j, key);
                return j.dump();
            }
        )
        .def_static("from_ed25519", &validation::Key::from_ed25519);

    py::class_<validation::RoleFullKeys>(m, "RoleFullKeys")
        .def(py::init<>())
        .def(
            py::init<const std::map<std::string, validation::Key>&, const std::size_t&>(),
            py::arg("keys"),
            py::arg("threshold")
        )
        .def_readwrite("keys", &validation::RoleFullKeys::keys)
        .def_readwrite("threshold", &validation::RoleFullKeys::threshold);

    py::class_<validation::TimeRef>(m, "TimeRef")
        .def(py::init<>())
        .def(py::init<std::time_t>())
        .def("set_now", &validation::TimeRef::set_now)
        .def("set", &validation::TimeRef::set)
        .def("timestamp", &validation::TimeRef::timestamp);

    py::class_<validation::SpecBase, std::shared_ptr<validation::SpecBase>>(m, "SpecBase");

    py::class_<validation::RoleBase, std::shared_ptr<validation::RoleBase>>(m, "RoleBase")
        .def_property_readonly("type", &validation::RoleBase::type)
        .def_property_readonly("version", &validation::RoleBase::version)
        .def_property_readonly("spec_version", &validation::RoleBase::spec_version)
        .def_property_readonly("file_ext", &validation::RoleBase::file_ext)
        .def_property_readonly("expires", &validation::RoleBase::expires)
        .def_property_readonly("expired", &validation::RoleBase::expired)
        .def("all_keys", &validation::RoleBase::all_keys);

    py::class_<validation::v06::V06RoleBaseExtension, std::shared_ptr<validation::v06::V06RoleBaseExtension>>(
        m,
        "RoleBaseExtension"
    )
        .def_property_readonly("timestamp", &validation::v06::V06RoleBaseExtension::timestamp);

    py::class_<validation::v06::SpecImpl, validation::SpecBase, std::shared_ptr<validation::v06::SpecImpl>>(
        m,
        "SpecImpl"
    )
        .def(py::init<>());

    py::class_<
        validation::v06::KeyMgrRole,
        validation::RoleBase,
        validation::v06::V06RoleBaseExtension,
        std::shared_ptr<validation::v06::KeyMgrRole>>(m, "KeyMgr")
        .def(py::init<
             const std::string&,
             const validation::RoleFullKeys&,
             const std::shared_ptr<validation::SpecBase>>());

    py::class_<
        validation::v06::PkgMgrRole,
        validation::RoleBase,
        validation::v06::V06RoleBaseExtension,
        std::shared_ptr<validation::v06::PkgMgrRole>>(m, "PkgMgr")
        .def(py::init<
             const std::string&,
             const validation::RoleFullKeys&,
             const std::shared_ptr<validation::SpecBase>>());

    py::class_<
        validation::v06::RootImpl,
        validation::RoleBase,
        validation::v06::V06RoleBaseExtension,
        std::shared_ptr<validation::v06::RootImpl>>(m, "RootImpl")
        .def(py::init<const std::string&>(), py::arg("json_str"))
        .def(
            "update",
            [](validation::v06::RootImpl& role, const std::string& json_str)
            { return role.update(nlohmann::json::parse(json_str)); },
            py::arg("json_str")
        )
        .def(
            "create_key_mgr",
            [](validation::v06::RootImpl& role, const std::string& json_str)
            { return role.create_key_mgr(nlohmann::json::parse(json_str)); },
            py::arg("json_str")
        );

    pyChannel
        .def(py::init(
            [](const std::string& value) {
                return const_cast<Channel*>(&mambapy::singletons.channel_context().make_channel(value)
                );
            }
        ))
        .def_property_readonly("location", &Channel::location)
        .def_property_readonly("name", &Channel::name)
        .def_property_readonly("platforms", &Channel::platforms)
        .def_property_readonly("canonical_name", &Channel::canonical_name)
        .def("urls", &Channel::urls, py::arg("with_credentials") = true)
        .def("platform_urls", &Channel::platform_urls, py::arg("with_credentials") = true)
        .def("platform_url", &Channel::platform_url, py::arg("platform"), py::arg("with_credentials") = true)
        .def(
            "__repr__",
            [](const Channel& c)
            {
                auto s = c.name();
                s += "[";
                bool first = true;
                for (const auto& platform : c.platforms())
                {
                    if (!first)
                    {
                        s += ",";
                    }
                    s += platform;
                    first = false;
                }
                s += "]";
                return s;
            }
        );

    m.def("clean", [](int flags) { return clean(mambapy::singletons.config(), flags); });

    m.def(
        "get_channels",
        [](const std::vector<std::string>& channel_names)
        { return mambapy::singletons.channel_context().get_channels(channel_names); }
    );

    m.def(
        "transmute",
        +[](const fs::u8path& pkg_file, const fs::u8path& target, int compression_level, int compression_threads
         )
        {
            const auto extract_options = mamba::ExtractOptions::from_context(
                mambapy::singletons.context()
            );
            return transmute(pkg_file, target, compression_level, compression_threads, extract_options);
        },
        py::arg("source_package"),
        py::arg("destination_package"),
        py::arg("compression_level"),
        py::arg("compression_threads") = 1
    );

    m.def("init_console", &init_console);

    // fix extract from error_handling first
    // auto package_handling_sm = m.def_submodule("package_handling");
    // package_handling_sm.def("extract", &extract);
    // package_handling_sm.def("create", &create_package, py::arg("directory"),
    // py::arg("out_package"), py::arg("compression_level"), py::arg("compression_threads") = 1);


    m.def("get_virtual_packages", [] { return get_virtual_packages(mambapy::singletons.context()); });

    m.def("cancel_json_output", [] { Console::instance().cancel_json_print(); });

    m.attr("SOLVER_SOLVABLE") = SOLVER_SOLVABLE;
    m.attr("SOLVER_SOLVABLE_NAME") = SOLVER_SOLVABLE_NAME;
    m.attr("SOLVER_SOLVABLE_PROVIDES") = SOLVER_SOLVABLE_PROVIDES;
    m.attr("SOLVER_SOLVABLE_ONE_OF") = SOLVER_SOLVABLE_ONE_OF;
    m.attr("SOLVER_SOLVABLE_REPO") = SOLVER_SOLVABLE_REPO;
    m.attr("SOLVER_SOLVABLE_ALL") = SOLVER_SOLVABLE_ALL;
    m.attr("SOLVER_SELECTMASK") = SOLVER_SELECTMASK;
    m.attr("SOLVER_NOOP") = SOLVER_NOOP;
    m.attr("SOLVER_INSTALL") = SOLVER_INSTALL;
    m.attr("SOLVER_ERASE") = SOLVER_ERASE;
    m.attr("SOLVER_UPDATE") = SOLVER_UPDATE;
    m.attr("SOLVER_WEAKENDEPS") = SOLVER_WEAKENDEPS;
    m.attr("SOLVER_MULTIVERSION") = SOLVER_MULTIVERSION;
    m.attr("SOLVER_LOCK") = SOLVER_LOCK;
    m.attr("SOLVER_DISTUPGRADE") = SOLVER_DISTUPGRADE;
    m.attr("SOLVER_VERIFY") = SOLVER_VERIFY;
    m.attr("SOLVER_DROP_ORPHANED") = SOLVER_DROP_ORPHANED;
    m.attr("SOLVER_USERINSTALLED") = SOLVER_USERINSTALLED;
    m.attr("SOLVER_ALLOWUNINSTALL") = SOLVER_ALLOWUNINSTALL;
    m.attr("SOLVER_FAVOR") = SOLVER_FAVOR;
    m.attr("SOLVER_DISFAVOR") = SOLVER_DISFAVOR;
    m.attr("SOLVER_JOBMASK") = SOLVER_JOBMASK;
    m.attr("SOLVER_WEAK") = SOLVER_WEAK;
    m.attr("SOLVER_ESSENTIAL") = SOLVER_ESSENTIAL;
    m.attr("SOLVER_CLEANDEPS") = SOLVER_CLEANDEPS;
    m.attr("SOLVER_ORUPDATE") = SOLVER_ORUPDATE;
    m.attr("SOLVER_FORCEBEST") = SOLVER_FORCEBEST;
    m.attr("SOLVER_TARGETED") = SOLVER_TARGETED;
    m.attr("SOLVER_NOTBYUSER") = SOLVER_NOTBYUSER;
    m.attr("SOLVER_SETEV") = SOLVER_SETEV;
    m.attr("SOLVER_SETEVR") = SOLVER_SETEVR;
    m.attr("SOLVER_SETARCH") = SOLVER_SETARCH;
    m.attr("SOLVER_SETVENDOR") = SOLVER_SETVENDOR;
    m.attr("SOLVER_SETREPO") = SOLVER_SETREPO;
    m.attr("SOLVER_NOAUTOSET") = SOLVER_NOAUTOSET;
    m.attr("SOLVER_SETNAME") = SOLVER_SETNAME;
    m.attr("SOLVER_SETMASK") = SOLVER_SETMASK;

    // Solver flags
    m.attr("SOLVER_FLAG_ALLOW_DOWNGRADE") = SOLVER_FLAG_ALLOW_DOWNGRADE;
    m.attr("SOLVER_FLAG_ALLOW_ARCHCHANGE") = SOLVER_FLAG_ALLOW_ARCHCHANGE;
    m.attr("SOLVER_FLAG_ALLOW_VENDORCHANGE") = SOLVER_FLAG_ALLOW_VENDORCHANGE;
    m.attr("SOLVER_FLAG_ALLOW_UNINSTALL") = SOLVER_FLAG_ALLOW_UNINSTALL;
    m.attr("SOLVER_FLAG_NO_UPDATEPROVIDE") = SOLVER_FLAG_NO_UPDATEPROVIDE;
    m.attr("SOLVER_FLAG_SPLITPROVIDES") = SOLVER_FLAG_SPLITPROVIDES;
    m.attr("SOLVER_FLAG_IGNORE_RECOMMENDED") = SOLVER_FLAG_IGNORE_RECOMMENDED;
    m.attr("SOLVER_FLAG_ADD_ALREADY_RECOMMENDED") = SOLVER_FLAG_ADD_ALREADY_RECOMMENDED;
    m.attr("SOLVER_FLAG_NO_INFARCHCHECK") = SOLVER_FLAG_NO_INFARCHCHECK;
    m.attr("SOLVER_FLAG_ALLOW_NAMECHANGE") = SOLVER_FLAG_ALLOW_NAMECHANGE;
    m.attr("SOLVER_FLAG_KEEP_EXPLICIT_OBSOLETES") = SOLVER_FLAG_KEEP_EXPLICIT_OBSOLETES;
    m.attr("SOLVER_FLAG_BEST_OBEY_POLICY") = SOLVER_FLAG_BEST_OBEY_POLICY;
    m.attr("SOLVER_FLAG_NO_AUTOTARGET") = SOLVER_FLAG_NO_AUTOTARGET;
    m.attr("SOLVER_FLAG_DUP_ALLOW_DOWNGRADE") = SOLVER_FLAG_DUP_ALLOW_DOWNGRADE;
    m.attr("SOLVER_FLAG_DUP_ALLOW_ARCHCHANGE") = SOLVER_FLAG_DUP_ALLOW_ARCHCHANGE;
    m.attr("SOLVER_FLAG_DUP_ALLOW_VENDORCHANGE") = SOLVER_FLAG_DUP_ALLOW_VENDORCHANGE;
    m.attr("SOLVER_FLAG_DUP_ALLOW_NAMECHANGE") = SOLVER_FLAG_DUP_ALLOW_NAMECHANGE;
    m.attr("SOLVER_FLAG_KEEP_ORPHANS") = SOLVER_FLAG_KEEP_ORPHANS;
    m.attr("SOLVER_FLAG_BREAK_ORPHANS") = SOLVER_FLAG_BREAK_ORPHANS;
    m.attr("SOLVER_FLAG_FOCUS_INSTALLED") = SOLVER_FLAG_FOCUS_INSTALLED;
    m.attr("SOLVER_FLAG_YUM_OBSOLETES") = SOLVER_FLAG_YUM_OBSOLETES;
    m.attr("SOLVER_FLAG_NEED_UPDATEPROVIDE") = SOLVER_FLAG_NEED_UPDATEPROVIDE;
    m.attr("SOLVER_FLAG_URPM_REORDER") = SOLVER_FLAG_URPM_REORDER;
    m.attr("SOLVER_FLAG_FOCUS_BEST") = SOLVER_FLAG_FOCUS_BEST;
    m.attr("SOLVER_FLAG_STRONG_RECOMMENDS") = SOLVER_FLAG_STRONG_RECOMMENDS;
    m.attr("SOLVER_FLAG_INSTALL_ALSO_UPDATES") = SOLVER_FLAG_INSTALL_ALSO_UPDATES;
    m.attr("SOLVER_FLAG_ONLY_NAMESPACE_RECOMMENDED") = SOLVER_FLAG_ONLY_NAMESPACE_RECOMMENDED;
    m.attr("SOLVER_FLAG_STRICT_REPO_PRIORITY") = SOLVER_FLAG_STRICT_REPO_PRIORITY;

    // Solver rule flags
    py::enum_<SolverRuleinfo>(m, "SolverRuleinfo")
        .value("SOLVER_RULE_UNKNOWN", SolverRuleinfo::SOLVER_RULE_UNKNOWN)
        .value("SOLVER_RULE_PKG", SolverRuleinfo::SOLVER_RULE_PKG)
        .value("SOLVER_RULE_PKG_NOT_INSTALLABLE", SolverRuleinfo::SOLVER_RULE_PKG_NOT_INSTALLABLE)
        .value("SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP", SolverRuleinfo::SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP)
        .value("SOLVER_RULE_PKG_REQUIRES", SolverRuleinfo::SOLVER_RULE_PKG_REQUIRES)
        .value("SOLVER_RULE_PKG_SELF_CONFLICT", SolverRuleinfo::SOLVER_RULE_PKG_SELF_CONFLICT)
        .value("SOLVER_RULE_PKG_CONFLICTS", SolverRuleinfo::SOLVER_RULE_PKG_CONFLICTS)
        .value("SOLVER_RULE_PKG_SAME_NAME", SolverRuleinfo::SOLVER_RULE_PKG_SAME_NAME)
        .value("SOLVER_RULE_PKG_OBSOLETES", SolverRuleinfo::SOLVER_RULE_PKG_OBSOLETES)
        .value("SOLVER_RULE_PKG_IMPLICIT_OBSOLETES", SolverRuleinfo::SOLVER_RULE_PKG_IMPLICIT_OBSOLETES)
        .value("SOLVER_RULE_PKG_INSTALLED_OBSOLETES", SolverRuleinfo::SOLVER_RULE_PKG_INSTALLED_OBSOLETES)
        .value("SOLVER_RULE_PKG_RECOMMENDS", SolverRuleinfo::SOLVER_RULE_PKG_RECOMMENDS)
        .value("SOLVER_RULE_PKG_CONSTRAINS", SolverRuleinfo::SOLVER_RULE_PKG_CONSTRAINS)
        .value("SOLVER_RULE_UPDATE", SolverRuleinfo::SOLVER_RULE_UPDATE)
        .value("SOLVER_RULE_FEATURE", SolverRuleinfo::SOLVER_RULE_FEATURE)
        .value("SOLVER_RULE_JOB", SolverRuleinfo::SOLVER_RULE_JOB)
        .value("SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP", SolverRuleinfo::SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP)
        .value("SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM", SolverRuleinfo::SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM)
        .value("SOLVER_RULE_JOB_UNKNOWN_PACKAGE", SolverRuleinfo::SOLVER_RULE_JOB_UNKNOWN_PACKAGE)
        .value("SOLVER_RULE_JOB_UNSUPPORTED", SolverRuleinfo::SOLVER_RULE_JOB_UNSUPPORTED)
        .value("SOLVER_RULE_DISTUPGRADE", SolverRuleinfo::SOLVER_RULE_DISTUPGRADE)
        .value("SOLVER_RULE_INFARCH", SolverRuleinfo::SOLVER_RULE_INFARCH)
        .value("SOLVER_RULE_CHOICE", SolverRuleinfo::SOLVER_RULE_CHOICE)
        .value("SOLVER_RULE_LEARNT", SolverRuleinfo::SOLVER_RULE_LEARNT)
        .value("SOLVER_RULE_BEST", SolverRuleinfo::SOLVER_RULE_BEST)
        .value("SOLVER_RULE_YUMOBS", SolverRuleinfo::SOLVER_RULE_YUMOBS)
        .value("SOLVER_RULE_RECOMMENDS", SolverRuleinfo::SOLVER_RULE_RECOMMENDS)
        .value("SOLVER_RULE_BLACK", SolverRuleinfo::SOLVER_RULE_BLACK)
        .value("SOLVER_RULE_STRICT_REPO_PRIORITY", SolverRuleinfo::SOLVER_RULE_STRICT_REPO_PRIORITY);

    // INSTALL FLAGS
    m.attr("MAMBA_NO_DEPS") = PY_MAMBA_NO_DEPS;
    m.attr("MAMBA_ONLY_DEPS") = PY_MAMBA_ONLY_DEPS;
    m.attr("MAMBA_FORCE_REINSTALL") = PY_MAMBA_FORCE_REINSTALL;

    // CLEAN FLAGS
    m.attr("MAMBA_CLEAN_ALL") = MAMBA_CLEAN_ALL;
    m.attr("MAMBA_CLEAN_INDEX") = MAMBA_CLEAN_INDEX;
    m.attr("MAMBA_CLEAN_PKGS") = MAMBA_CLEAN_PKGS;
    m.attr("MAMBA_CLEAN_TARBALLS") = MAMBA_CLEAN_TARBALLS;
    m.attr("MAMBA_CLEAN_LOCKS") = MAMBA_CLEAN_LOCKS;
}

namespace mamba::bindings
{
    void bind_submodule(pybind11::module_ m)
    {
        bind_submodule_impl(std::move(m));
    }
}
