// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/query.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/core/subdirdata.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/url.hpp"
#include "mamba/core/util.hpp"

namespace py = pybind11;

namespace query
{
    enum RESULT_FORMAT
    {
        JSON,
        TREE,
        TABLE,
        PRETTY
    };
}

PYBIND11_MODULE(mamba_api, m)
{
    using namespace mamba;

    py::class_<fs::path>(m, "Path")
        .def(py::init<std::string>())
        .def("__repr__", [](fs::path& self) -> std::string {
            return std::string("fs::path[") + std::string(self) + "]";
        });
    py::implicitly_convertible<std::string, fs::path>();

    py::register_exception<mamba_error>(m, "MambaNativeException");

    py::add_ostream_redirect(m, "ostream_redirect");

    py::class_<MPool>(m, "Pool")
        .def(py::init<>())
        .def("set_debuglevel", &MPool::set_debuglevel)
        .def("create_whatprovides", &MPool::create_whatprovides);

    py::class_<MultiPackageCache>(m, "MultiPackageCache")
        .def(py::init<std::vector<fs::path>>())
        .def("query", &MultiPackageCache::query);

    py::class_<MRepo>(m, "Repo")
        .def(py::init<MPool&, const std::string&, const std::string&, const std::string&>())
        .def(py::init<MPool&, const PrefixData&>())
        .def("set_installed", &MRepo::set_installed)
        .def("set_priority", &MRepo::set_priority)
        .def("name", &MRepo::name)
        .def("priority", &MRepo::priority)
        .def("size", &MRepo::size)
        .def("clear", &MRepo::clear);

    py::class_<MTransaction>(m, "Transaction")
        .def(py::init<MSolver&, MultiPackageCache&, const std::string&>())
        .def("to_conda", &MTransaction::to_conda)
        .def("log_json", &MTransaction::log_json)
        .def("print", &MTransaction::print)
        .def("fetch_extract_packages", &MTransaction::fetch_extract_packages)
        .def("prompt", &MTransaction::prompt)
        .def("execute", [](MTransaction& self, PrefixData& target_prefix) -> bool {
            return self.execute(target_prefix);
        });

    py::class_<MSolver>(m, "Solver")
        .def(py::init<MPool&, std::vector<std::pair<int, int>>>())
        .def(py::init<MPool&, std::vector<std::pair<int, int>>, const PrefixData*>())
        .def("add_jobs", &MSolver::add_jobs)
        .def("add_constraint", &MSolver::add_constraint)
        .def("add_pin", &MSolver::add_pin)
        .def("set_flags", &MSolver::set_flags)
        .def("set_postsolve_flags", &MSolver::set_postsolve_flags)
        .def("is_solved", &MSolver::is_solved)
        .def("problems_to_str", &MSolver::problems_to_str)
        .def("solve", &MSolver::solve);

    py::class_<History>(m, "History")
        .def(py::init<const std::string&>())
        .def("get_requested_specs_map", &History::get_requested_specs_map);

    py::class_<MatchSpec>(m, "MatchSpec")
        .def(py::init<>())
        .def(py::init<const std::string&>())
        .def("conda_build_form", &MatchSpec::conda_build_form);

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
        .value("PRETTY", query::RESULT_FORMAT::PRETTY);

    py::class_<Query>(m, "Query")
        .def(py::init<MPool&>())
        .def("find",
             [](const Query& q,
                const std::string& query,
                const query::RESULT_FORMAT format) -> std::string {
                 std::stringstream res_stream;
                 switch (format)
                 {
                     case query::JSON:
                         res_stream << q.find(query).groupby("name").json().dump(4);
                         break;
                     case query::TREE:
                     case query::TABLE:
                         q.find(query).groupby("name").table(res_stream);
                         break;
                     case query::PRETTY:
                         q.find(query).groupby("name").pretty(res_stream);
                 }
                 return res_stream.str();
             })
        .def("whoneeds",
             [](const Query& q,
                const std::string& query,
                const query::RESULT_FORMAT format) -> std::string {
                 // QueryResult res = q.whoneeds(query, tree);
                 std::stringstream res_stream;
                 query_result res = q.whoneeds(query, (format == query::TREE));
                 switch (format)
                 {
                     case query::TREE:
                         res.tree(res_stream);
                         break;
                     case query::JSON:
                         res_stream << res.json().dump(4);
                         break;
                     case query::TABLE:
                         res.table(
                             res_stream,
                             { "Name", "Version", "Build", concat("Depends:", query), "Channel" });
                 }
                 return res_stream.str();
             })
        .def("depends",
             [](const Query& q,
                const std::string& query,
                const query::RESULT_FORMAT format) -> std::string {
                 query_result res = q.depends(query, (format == query::TREE));
                 std::stringstream res_stream;
                 switch (format)
                 {
                     case query::TREE:
                         res.tree(res_stream);
                         break;
                     case query::JSON:
                         res_stream << res.json().dump(4);
                         break;
                     case query::TABLE:
                         // res.table(res_stream, {"Name", "Version", "Build", concat("Depends:",
                         // query), "Channel"});
                         res.table(res_stream);
                 }
                 return res_stream.str();
             });

    py::class_<MSubdirData>(m, "SubdirData")
        .def(py::init<const std::string&, const std::string&, const std::string&, bool>())
        .def("create_repo", &MSubdirData::create_repo)
        .def("load", &MSubdirData::load)
        .def("loaded", &MSubdirData::loaded)
        .def("cache_path", &MSubdirData::cache_path);

    m.def("cache_fn_url", &cache_fn_url);
    m.def("create_cache_dir", &create_cache_dir);

    py::class_<MultiDownloadTarget>(m, "DownloadTargetList")
        .def(py::init<>())
        .def("add",
             [](MultiDownloadTarget& self, MSubdirData& sub) -> void { self.add(sub.target()); })
        .def("download", &MultiDownloadTarget::download);

    py::enum_<ChannelPriority>(m, "ChannelPriority")
        .value("kFlexible", ChannelPriority::kFlexible)
        .value("kStrict", ChannelPriority::kStrict)
        .value("kDisabled", ChannelPriority::kDisabled);

    py::class_<Context, std::unique_ptr<Context, py::nodelete>>(m, "Context")
        .def(
            py::init([]() { return std::unique_ptr<Context, py::nodelete>(&Context::instance()); }))
        .def_readwrite("verbosity", &Context::verbosity)
        .def_readwrite("quiet", &Context::quiet)
        .def_readwrite("json", &Context::json)
        .def_readwrite("offline", &Context::offline)
        .def_readwrite("local_repodata_ttl", &Context::local_repodata_ttl)
        .def_readwrite("use_index_cache", &Context::use_index_cache)
        .def_readwrite("max_parallel_downloads", &Context::max_parallel_downloads)
        .def_readwrite("always_yes", &Context::always_yes)
        .def_readwrite("dry_run", &Context::dry_run)
        .def_readwrite("ssl_verify", &Context::ssl_verify)
        .def_readwrite("max_retries", &Context::max_retries)
        .def_readwrite("retry_timeout", &Context::retry_timeout)
        .def_readwrite("retry_backoff", &Context::retry_backoff)
        // .def_readwrite("read_timeout_secs", &Context::read_timeout_secs)
        .def_readwrite("connect_timeout_secs", &Context::connect_timeout_secs)
        .def_readwrite("add_pip_as_python_dependency", &Context::add_pip_as_python_dependency)
        .def_readwrite("target_prefix", &Context::target_prefix)
        .def_readwrite("conda_prefix", &Context::conda_prefix)
        .def_readwrite("root_prefix", &Context::root_prefix)
        .def_readwrite("envs_dirs", &Context::envs_dirs)
        .def_readwrite("pkgs_dirs", &Context::pkgs_dirs)
        .def_readwrite("platform", &Context::platform)
        .def("set_verbosity", &Context::set_verbosity)
        .def_readwrite("channels", &Context::channels)
        .def_readwrite("custom_channels", &Context::custom_channels)
        .def_readwrite("default_channels", &Context::default_channels)
        .def_readwrite("channel_alias", &Context::channel_alias)
        .def_readwrite("use_only_tar_bz2", &Context::use_only_tar_bz2)
        .def_readwrite("channel_priority", &Context::channel_priority);

    py::class_<PrefixData>(m, "PrefixData")
        .def(py::init<const std::string&>())
        .def_readwrite("package_records", &PrefixData::m_package_records)
        .def("load", &PrefixData::load);

    py::class_<PackageInfo>(m, "PackageInfo")
        .def(py::init<Solvable*>())
        .def(py::init<const std::string&>())
        .def(py::init<const std::string&, const std::string&, const std::string&, std::size_t>())
        .def_readwrite("name", &PackageInfo::name);

    py::class_<Channel, std::unique_ptr<Channel, py::nodelete>>(m, "Channel")
        .def(py::init(
            [](const std::string& value) { return const_cast<Channel*>(&make_channel(value)); }))
        .def_property_readonly("scheme", &Channel::scheme)
        .def_property_readonly("location", &Channel::location)
        .def_property_readonly("name", &Channel::name)
        .def_property_readonly("auth", &Channel::auth)
        .def_property_readonly("token", &Channel::token)
        .def_property_readonly("package_filename", &Channel::package_filename)
        .def_property_readonly("platforms", &Channel::platforms)
        .def_property_readonly("canonical_name", &Channel::canonical_name)
        .def("urls", &Channel::urls, py::arg("with_credentials") = true)
        .def("platform_urls", &Channel::platform_urls, py::arg("with_credentials") = true)
        .def("platform_url",
             &Channel::platform_url,
             py::arg("platform"),
             py::arg("with_credentials") = true)
        .def("__repr__", [](const Channel& c) {
            auto s = c.name();
            s += "[";
            bool first = true;
            for (const auto& platform : c.platforms())
            {
                if (!first)
                    s += ",";
                s += platform;
                first = false;
            }
            s += "]";
            return s;
        });

    m.def("get_channels", &get_channels);

    m.def("transmute", &transmute);

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

    m.attr("MAMBA_NO_DEPS") = MAMBA_NO_DEPS;
    m.attr("MAMBA_ONLY_DEPS") = MAMBA_ONLY_DEPS;
    m.attr("MAMBA_FORCE_REINSTALL") = MAMBA_FORCE_REINSTALL;
}
