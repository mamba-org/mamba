// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "nlohmann/json.hpp"

// Make sure to use external fmt if desired
#include "spdlog/tweakme.h"
#include "spdlog/fmt/fmt.h"

#include "mamba/api/clean.hpp"
#include "mamba/api/configuration.hpp"

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
#include "mamba/core/validate.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/execution.hpp"

#include <stdexcept>

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

PYBIND11_MODULE(bindings, m)
{
    using namespace mamba;

    // declare earlier to avoid C++ types in docstrings
    auto pyChannel = py::class_<Channel, std::unique_ptr<Channel, py::nodelete>>(m, "Channel");
    auto pyPackageInfo = py::class_<PackageInfo>(m, "PackageInfo");
    auto pyPrefixData = py::class_<PrefixData>(m, "PrefixData");
    auto pySolver = py::class_<MSolver>(m, "Solver");
    // only used in a return type; does it belong in the module?
    auto pyRootRole = py::class_<validate::RootRole>(m, "RootRole");

    py::class_<fs::u8path>(m, "Path")
        .def(py::init<std::string>())
        .def("__str__", [](fs::u8path& self) -> std::string { return self.string(); })
        .def("__repr__",
             [](fs::u8path& self) -> std::string
             { return fmt::format("fs::u8path[{}]", self.string()); });
    py::implicitly_convertible<std::string, fs::u8path>();

    py::class_<mamba::LockFile>(m, "LockFile").def(py::init(&mamba::LockFile::create_lock));

    py::register_exception<mamba_error>(m, "MambaNativeException");

    py::add_ostream_redirect(m, "ostream_redirect");

    py::class_<MPool>(m, "Pool")
        .def(py::init<>())
        .def("set_debuglevel", &MPool::set_debuglevel)
        .def("create_whatprovides", &MPool::create_whatprovides)
        .def("select_solvables", &MPool::select_solvables, py::arg("id"))
        .def("matchspec2id", &MPool::matchspec2id, py::arg("ms"))
        .def("id2pkginfo", &MPool::id2pkginfo, py::arg("id"));

    py::class_<MultiPackageCache>(m, "MultiPackageCache")
        .def(py::init<std::vector<fs::u8path>>())
        .def("get_tarball_path", &MultiPackageCache::get_tarball_path)
        .def_property_readonly("first_writable_path", &MultiPackageCache::first_writable_path);

    struct ExtraPkgInfo
    {
        std::string noarch;
        std::string repo_url;
    };

    py::class_<ExtraPkgInfo>(m, "ExtraPkgInfo")
        .def(py::init<>())
        .def_readwrite("noarch", &ExtraPkgInfo::noarch)
        .def_readwrite("repo_url", &ExtraPkgInfo::repo_url);

    py::class_<MRepo, std::unique_ptr<MRepo, py::nodelete>>(m, "Repo")
        .def(py::init(
            [](MPool& pool,
               const std::string& name,
               const std::string& filename,
               const std::string& url) {
                return std::unique_ptr<MRepo, py::nodelete>(
                    &MRepo::create(pool, name, filename, url));
            }))
        .def(py::init([](MPool& pool, const PrefixData& data)
                      { return std::unique_ptr<MRepo, py::nodelete>(&MRepo::create(pool, data)); }))
        .def("add_extra_pkg_info",
             [](const MRepo& self, const std::map<std::string, ExtraPkgInfo>& additional_info)
             {
                 Id pkg_id;
                 Solvable* pkg_s;
                 Pool* p = self.repo()->pool;
                 static Id noarch_repo_key = pool_str2id(p, "solvable:noarch_type", 1);
                 static Id real_repo_url_key = pool_str2id(p, "solvable:real_repo_url", 1);

                 FOR_REPO_SOLVABLES(self.repo(), pkg_id, pkg_s)
                 {
                     std::string name = pool_id2str(p, pkg_s->name);
                     auto it = additional_info.find(name);
                     if (it != additional_info.end())
                     {
                         if (!it->second.noarch.empty())
                             solvable_set_str(pkg_s, noarch_repo_key, it->second.noarch.c_str());
                         if (!it->second.repo_url.empty())
                             solvable_set_str(
                                 pkg_s, real_repo_url_key, it->second.repo_url.c_str());
                     }
                 }
                 repo_internalize(self.repo());
             })
        .def("set_installed", &MRepo::set_installed)
        .def("set_priority", &MRepo::set_priority)
        .def("name", &MRepo::name)
        .def("priority", &MRepo::priority)
        .def("size", &MRepo::size)
        .def("clear", &MRepo::clear);

    py::class_<MTransaction>(m, "Transaction")
        .def(py::init<MSolver&, MultiPackageCache&>())
        .def("to_conda", &MTransaction::to_conda)
        .def("log_json", &MTransaction::log_json)
        .def("print", &MTransaction::print)
        .def("fetch_extract_packages", &MTransaction::fetch_extract_packages)
        .def("prompt", &MTransaction::prompt)
        .def("find_python_version", &MTransaction::find_python_version)
        .def("execute", &MTransaction::execute);

    pySolver.def(py::init<MPool&, std::vector<std::pair<int, int>>>(), py::keep_alive<1, 2>())
        .def("add_jobs", &MSolver::add_jobs)
        .def("add_global_job", &MSolver::add_global_job)
        .def("add_constraint", &MSolver::add_constraint)
        .def("add_pin", &MSolver::add_pin)
        .def("set_flags", &MSolver::set_flags)
        .def("set_postsolve_flags", &MSolver::set_postsolve_flags)
        .def("is_solved", &MSolver::is_solved)
        .def("problems_to_str", &MSolver::problems_to_str)
        .def("all_problems_to_str", &MSolver::all_problems_to_str)
        .def("all_problems_structured", &MSolver::all_problems_structured)
        .def("solve", &MSolver::solve);

    py::class_<MSolverProblem>(m, "SolverProblem")
        .def_readwrite("target_id", &MSolverProblem::target_id)
        .def_readwrite("source_id", &MSolverProblem::source_id)
        .def_readwrite("dep_id", &MSolverProblem::dep_id)
        .def_readwrite("type", &MSolverProblem::type)
        .def("__str__", &MSolverProblem::to_string)
        .def("target", &MSolverProblem::target)
        .def("source", &MSolverProblem::source)
        .def("dep", &MSolverProblem::dep);

    py::class_<History>(m, "History")
        .def(py::init<const fs::u8path&>())
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
                const query::RESULT_FORMAT format) -> std::string
             {
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
                const query::RESULT_FORMAT format) -> std::string
             {
                 // QueryResult res = q.whoneeds(query, tree);
                 std::stringstream res_stream;
                 query_result res = q.whoneeds(query, (format == query::TREE));
                 switch (format)
                 {
                     case query::TREE:
                     case query::PRETTY:
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
                const query::RESULT_FORMAT format) -> std::string
             {
                 query_result res = q.depends(query, (format == query::TREE));
                 std::stringstream res_stream;
                 switch (format)
                 {
                     case query::TREE:
                     case query::PRETTY:
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
        .def(py::init(
            [](const Channel& channel,
               const std::string& platform,
               const std::string& url,
               MultiPackageCache& caches,
               const std::string& repodata_fn) -> MSubdirData
            {
                auto sres = MSubdirData::create(channel, platform, url, caches, repodata_fn);
                return extract(std::move(sres));
            }))
        .def(
            "create_repo",
            [](MSubdirData& subdir, MPool& pool) -> MRepo&
            {
                auto exp_res = subdir.create_repo(pool);
                return extract(exp_res);
            },
            py::return_value_policy::reference)
        .def("loaded", &MSubdirData::loaded)
        .def("cache_path",
             [](const MSubdirData& self) -> std::string { return extract(self.cache_path()); });

    m.def("cache_fn_url", &cache_fn_url);
    m.def("create_cache_dir", &create_cache_dir);

    // py::class_<MultiDownloadTarget>(m, "DownloadTargetList")
    //     .def(py::init<>())
    //     .def("add",
    //          [](MultiDownloadTarget& self, MSubdirData& sub) -> void { self.add(sub.target()); })
    //     .def("download", &MultiDownloadTarget::download);

    py::enum_<ChannelPriority>(m, "ChannelPriority")
        .value("kFlexible", ChannelPriority::kFlexible)
        .value("kStrict", ChannelPriority::kStrict)
        .value("kDisabled", ChannelPriority::kDisabled);

    py::enum_<mamba::log_level>(m, "LogLevel")
        .value("TRACE", mamba::log_level::trace)
        .value("DEBUG", mamba::log_level::debug)
        .value("INFO", mamba::log_level::info)
        .value("WARNING", mamba::log_level::warn)
        .value("ERROR", mamba::log_level::err)
        .value("CRITICAL", mamba::log_level::critical)
        .value("OFF", mamba::log_level::off);

    py::class_<Context, std::unique_ptr<Context, py::nodelete>>(m, "Context")
        .def(
            py::init([]() { return std::unique_ptr<Context, py::nodelete>(&Context::instance()); }))
        .def_readwrite("verbosity", &Context::verbosity)
        .def_readwrite("quiet", &Context::quiet)
        .def_readwrite("json", &Context::json)
        .def_readwrite("offline", &Context::offline)
        .def_readwrite("local_repodata_ttl", &Context::local_repodata_ttl)
        .def_readwrite("use_index_cache", &Context::use_index_cache)
        .def_readwrite("download_threads", &Context::download_threads)
        .def_readwrite("extract_threads", &Context::extract_threads)
        .def_readwrite("always_yes", &Context::always_yes)
        .def_readwrite("dry_run", &Context::dry_run)
        .def_readwrite("ssl_verify", &Context::ssl_verify)
        .def_readwrite("proxy_servers", &Context::proxy_servers)
        .def_readwrite("max_retries", &Context::max_retries)
        .def_readwrite("retry_timeout", &Context::retry_timeout)
        .def_readwrite("retry_backoff", &Context::retry_backoff)
        .def_readwrite("user_agent", &Context::user_agent)
        // .def_readwrite("read_timeout_secs", &Context::read_timeout_secs)
        .def_readwrite("connect_timeout_secs", &Context::connect_timeout_secs)
        .def_readwrite("add_pip_as_python_dependency", &Context::add_pip_as_python_dependency)
        .def_readwrite("target_prefix", &Context::target_prefix)
        .def_readwrite("conda_prefix", &Context::conda_prefix)
        .def_readwrite("root_prefix", &Context::root_prefix)
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
        .def("set_verbosity", &Context::set_verbosity)
        .def("set_log_level", &Context::set_log_level);

    pyPrefixData
        .def(py::init(
            [](const fs::u8path& prefix_path) -> PrefixData
            {
                auto sres = PrefixData::create(prefix_path);
                if (sres.has_value())
                {
                    return std::move(sres.value());
                }
                else
                {
                    throw sres.error();
                }
            }))
        .def_property_readonly("package_records", &PrefixData::records)
        .def("add_packages", &PrefixData::add_packages);

    pyPackageInfo.def(py::init<Solvable*>())
        .def(py::init<const std::string&>(), py::arg("name"))
        .def(py::init<const std::string&, const std::string&, const std::string&, std::size_t>(),
             py::arg("name"),
             py::arg("version"),
             py::arg("build_string"),
             py::arg("build_number"))
        .def_readwrite("name", &PackageInfo::name)
        .def_readwrite("version", &PackageInfo::version)
        .def_readwrite("build_string", &PackageInfo::build_string)
        .def_readwrite("build_number", &PackageInfo::build_number)
        .def_readwrite("channel", &PackageInfo::channel)
        .def_readwrite("url", &PackageInfo::url)
        .def_readwrite("subdir", &PackageInfo::subdir)
        .def_readwrite("fn", &PackageInfo::fn)
        .def_readwrite("license", &PackageInfo::license)
        .def_readwrite("size", &PackageInfo::size)
        .def_readwrite("timestamp", &PackageInfo::timestamp)
        .def_readwrite("md5", &PackageInfo::md5)
        .def_readwrite("sha256", &PackageInfo::sha256)
        .def_readwrite("track_features", &PackageInfo::track_features)
        .def_readwrite("depends", &PackageInfo::depends)
        .def_readwrite("constrains", &PackageInfo::constrains)
        .def_readwrite("signatures", &PackageInfo::signatures)
        .def_readwrite("extra_metadata", &PackageInfo::extra_metadata)
        .def_readwrite("defaulted_keys", &PackageInfo::defaulted_keys);

    // Content trust - Package signature and verification
    m.def("generate_ed25519_keypair", &validate::generate_ed25519_keypair_hex);
    m.def(
        "sign",
        [](const std::string& data, const std::string& sk)
        {
            std::string signature;
            if (!validate::sign(data, sk, signature))
                throw std::runtime_error("Signing failed");
            return signature;
        },
        py::arg("data"),
        py::arg("secret_key"));

    py::class_<validate::Key>(m, "Key")
        .def_readwrite("keytype", &validate::Key::keytype)
        .def_readwrite("scheme", &validate::Key::scheme)
        .def_readwrite("keyval", &validate::Key::keyval)
        .def_property_readonly("json_str",
                               [](const validate::Key& key)
                               {
                                   nlohmann::json j;
                                   validate::to_json(j, key);
                                   return j.dump();
                               })
        .def_static("from_ed25519", &validate::Key::from_ed25519);

    py::class_<validate::RoleFullKeys>(m, "RoleFullKeys")
        .def(py::init<>())
        .def(py::init<const std::map<std::string, validate::Key>&, const std::size_t&>(),
             py::arg("keys"),
             py::arg("threshold"))
        .def_readwrite("keys", &validate::RoleFullKeys::keys)
        .def_readwrite("threshold", &validate::RoleFullKeys::threshold);

    py::class_<validate::SpecBase, std::shared_ptr<validate::SpecBase>>(m, "SpecBase");

    py::class_<validate::RoleBase, std::shared_ptr<validate::RoleBase>>(m, "RoleBase")
        .def_property_readonly("type", &validate::RoleBase::type)
        .def_property_readonly("version", &validate::RoleBase::version)
        .def_property_readonly("spec_version", &validate::RoleBase::spec_version)
        .def_property_readonly("file_ext", &validate::RoleBase::file_ext)
        .def_property_readonly("expires", &validate::RoleBase::expires)
        .def_property_readonly("expired", &validate::RoleBase::expired)
        .def("all_keys", &validate::RoleBase::all_keys);

    py::class_<validate::v06::V06RoleBaseExtension,
               std::shared_ptr<validate::v06::V06RoleBaseExtension>>(m, "RoleBaseExtension")
        .def_property_readonly("timestamp", &validate::v06::V06RoleBaseExtension::timestamp);

    py::class_<validate::v06::SpecImpl,
               validate::SpecBase,
               std::shared_ptr<validate::v06::SpecImpl>>(m, "SpecImpl")
        .def(py::init<>());

    py::class_<validate::v06::KeyMgrRole,
               validate::RoleBase,
               validate::v06::V06RoleBaseExtension,
               std::shared_ptr<validate::v06::KeyMgrRole>>(m, "KeyMgr")
        .def(py::init<const std::string&,
                      const validate::RoleFullKeys&,
                      const std::shared_ptr<validate::SpecBase>>());

    py::class_<validate::v06::PkgMgrRole,
               validate::RoleBase,
               validate::v06::V06RoleBaseExtension,
               std::shared_ptr<validate::v06::PkgMgrRole>>(m, "PkgMgr")
        .def(py::init<const std::string&,
                      const validate::RoleFullKeys&,
                      const std::shared_ptr<validate::SpecBase>>());

    py::class_<validate::v06::RootImpl,
               validate::RoleBase,
               validate::v06::V06RoleBaseExtension,
               std::shared_ptr<validate::v06::RootImpl>>(m, "RootImpl")
        .def(py::init<const std::string&>(), py::arg("json_str"))
        .def(
            "update",
            [](validate::v06::RootImpl& role, const std::string& json_str)
            { return role.update(nlohmann::json::parse(json_str)); },
            py::arg("json_str"))
        .def(
            "create_key_mgr",
            [](validate::v06::RootImpl& role, const std::string& json_str)
            { return role.create_key_mgr(nlohmann::json::parse(json_str)); },
            py::arg("json_str"));

    pyChannel
        .def(py::init([](const std::string& value)
                      { return const_cast<Channel*>(&make_channel(value)); }))
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
        .def("__repr__",
             [](const Channel& c)
             {
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

    m.def("clean", &clean);

    py::class_<Configuration, std::unique_ptr<Configuration, py::nodelete>>(m, "Configuration")
        .def(py::init(
            []()
            { return std::unique_ptr<Configuration, py::nodelete>(&Configuration::instance()); }))
        .def_property(
            "show_banner",
            []() -> bool { return Configuration::instance().at("show_banner").value<bool>(); },
            [](py::object&, bool val)
            { Configuration::instance().at("show_banner").set_value(val); });

    m.def("get_channels", &get_channels);

    m.def("transmute", &transmute);

    m.def("get_virtual_packages", &get_virtual_packages);

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
        .value("SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP",
               SolverRuleinfo::SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP)
        .value("SOLVER_RULE_PKG_REQUIRES", SolverRuleinfo::SOLVER_RULE_PKG_REQUIRES)
        .value("SOLVER_RULE_PKG_SELF_CONFLICT", SolverRuleinfo::SOLVER_RULE_PKG_SELF_CONFLICT)
        .value("SOLVER_RULE_PKG_CONFLICTS", SolverRuleinfo::SOLVER_RULE_PKG_CONFLICTS)
        .value("SOLVER_RULE_PKG_SAME_NAME", SolverRuleinfo::SOLVER_RULE_PKG_SAME_NAME)
        .value("SOLVER_RULE_PKG_OBSOLETES", SolverRuleinfo::SOLVER_RULE_PKG_OBSOLETES)
        .value("SOLVER_RULE_PKG_IMPLICIT_OBSOLETES",
               SolverRuleinfo::SOLVER_RULE_PKG_IMPLICIT_OBSOLETES)
        .value("SOLVER_RULE_PKG_INSTALLED_OBSOLETES",
               SolverRuleinfo::SOLVER_RULE_PKG_INSTALLED_OBSOLETES)
        .value("SOLVER_RULE_PKG_RECOMMENDS", SolverRuleinfo::SOLVER_RULE_PKG_RECOMMENDS)
        .value("SOLVER_RULE_PKG_CONSTRAINS", SolverRuleinfo::SOLVER_RULE_PKG_CONSTRAINS)
        .value("SOLVER_RULE_UPDATE", SolverRuleinfo::SOLVER_RULE_UPDATE)
        .value("SOLVER_RULE_FEATURE", SolverRuleinfo::SOLVER_RULE_FEATURE)
        .value("SOLVER_RULE_JOB", SolverRuleinfo::SOLVER_RULE_JOB)
        .value("SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP",
               SolverRuleinfo::SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP)
        .value("SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM",
               SolverRuleinfo::SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM)
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
        .value("SOLVER_RULE_STRICT_REPO_PRIORITY",
               SolverRuleinfo::SOLVER_RULE_STRICT_REPO_PRIORITY);

    // INSTALL FLAGS
    m.attr("MAMBA_NO_DEPS") = MAMBA_NO_DEPS;
    m.attr("MAMBA_ONLY_DEPS") = MAMBA_ONLY_DEPS;
    m.attr("MAMBA_FORCE_REINSTALL") = MAMBA_FORCE_REINSTALL;

    // DOWNLOAD FLAGS
    m.attr("MAMBA_DOWNLOAD_FAILFAST") = MAMBA_DOWNLOAD_FAILFAST;
    m.attr("MAMBA_DOWNLOAD_SORT") = MAMBA_DOWNLOAD_SORT;

    // CLEAN FLAGS
    m.attr("MAMBA_CLEAN_ALL") = MAMBA_CLEAN_ALL;
    m.attr("MAMBA_CLEAN_INDEX") = MAMBA_CLEAN_INDEX;
    m.attr("MAMBA_CLEAN_PKGS") = MAMBA_CLEAN_PKGS;
    m.attr("MAMBA_CLEAN_TARBALLS") = MAMBA_CLEAN_TARBALLS;
    m.attr("MAMBA_CLEAN_LOCKS") = MAMBA_CLEAN_LOCKS;
}
