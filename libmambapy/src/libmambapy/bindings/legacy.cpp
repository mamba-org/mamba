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

#include "mamba/api/clean.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/repoquery.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/context_params.hpp"
#include "mamba/core/download_progress_bar.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/query.hpp"
#include "mamba/core/subdir_index.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/solver/problems_graph.hpp"
#include "mamba/validation/tools.hpp"
#include "mamba/validation/update_framework_v0_6.hpp"

#include "bind_utils.hpp"
#include "bindings.hpp"
#include "expected_caster.hpp"
#include "flat_set_caster.hpp"
#include "path_caster.hpp"

namespace py = pybind11;

void
deprecated(std::string_view message, std::string_view since_version = "1.5")
{
    const auto warnings = py::module_::import("warnings");
    const auto builtins = py::module_::import("builtins");
    auto total_message = fmt::format("Deprecated since version {}: {}", since_version, message);
    warnings.attr("warn")(total_message, builtins.attr("DeprecationWarning"), py::arg("stacklevel") = 2);
}

namespace mambapy
{
    // When using this library we for now still need to have a few singletons available
    // to avoid the python code to have to create 3-4 objects before starting to work with
    // mamba functions. Instead, here, we associate the lifetime of all the necessary
    // singletons to the lifetime of the Context. This is to provide to the user explicit
    // control over the lifetime and construction options of the Context and library
    // resources, preventing issues related to default configuration/options.
    // In the future, we might remove all singletons and provide a simple way to start
    // working with mamba, but the C++ side needs to be made 100% singleton-less first.
    //
    // In the code below we provide a mechanism to associate the lifetime of the
    // necessary singletons to the lifetime of one Context instance and forbid more
    //  instances in the case of Python (it is theoretically allowed by the C++ api).


    class Singletons
    {
    public:

        explicit Singletons(mamba::ContextOptions options)
            : m_context(std::move(options))
        {
        }

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

        mamba::Configuration& config()
        {
            return m_config;
        }

    private:


        mamba::MainExecutor m_main_executor;
        mamba::Context m_context;
        mamba::Console m_console{ m_context };
        mamba::Configuration m_config{ m_context };
    };

    std::unique_ptr<Singletons> current_singletons;

    Singletons& singletons()
    {
        if (current_singletons == nullptr)
        {
            throw std::runtime_error("Context instance must be created first");
        }
        return *current_singletons;
    }

    struct destroy_singleton
    {
        template <class... Args>
        void operator()(Args&&...) noexcept
        {
            current_singletons.reset();
        }
    };

    static constexpr auto default_context_options = mamba::ContextOptions{ true, true };

    // MSubdirData objects are movable only, and they need to be moved into
    // a std::vector before we call MSudbirData::download. Since we cannot
    // replicate the move semantics in Python, we encapsulate the creation
    // and the storage of MSubdirData objects in this class, to avoid
    // potential dangling references in Python.
    //
    // Deprecated, replaced by SubdirIndexLoader in 2.3.0
    class SubdirIndex
    {
    public:

        struct Entry
        {
            mamba::SubdirIndexLoader* p_subdirdata = nullptr;
            std::string m_platform = "";
            const mamba::specs::Channel* p_channel = nullptr;
            std::string m_url = "";
        };

        using entry_list = std::vector<Entry>;
        using iterator = entry_list::const_iterator;

        // TODO: remove full_url from API
        void create(
            mamba::Context& ctx,
            mamba::ChannelContext& channel_context,
            const mamba::specs::Channel& channel,
            const std::string& platform,
            const std::string& /*full_url*/,
            mamba::MultiPackageCache& caches,
            const std::string& repodata_fn,
            const std::string& url
        )
        {
            using namespace mamba;
            auto subdir_params = ctx.subdir_params();
            subdir_params.repodata_force_use_zst = channel_context.has_zst(channel);
            m_subdirs.push_back(extract(
                SubdirIndexLoader::create(subdir_params, channel, platform, caches, repodata_fn)
            ));
            m_entries.push_back({ nullptr, platform, &channel, url });
            for (size_t i = 0; i < m_subdirs.size(); ++i)
            {
                m_entries[i].p_subdirdata = &(m_subdirs[i]);
            }
        }

        bool download(mamba::Context& ctx)
        {
            using namespace mamba;
            // TODO: expose SubdirDataMonitor to libmambapy and remove this
            //  logic
            expected_t<void> download_res;
            if (SubdirIndexMonitor::can_monitor(ctx))
            {
                SubdirIndexMonitor check_monitor({ true, true });
                SubdirIndexMonitor index_monitor;
                download_res = SubdirIndexLoader::download_required_indexes(
                    m_subdirs,
                    ctx.subdir_download_params(),
                    ctx.authentication_info(),
                    ctx.mirrors,
                    ctx.download_options(),
                    ctx.remote_fetch_params,
                    &check_monitor,
                    &index_monitor
                );
            }
            else
            {
                download_res = SubdirIndexLoader::download_required_indexes(
                    m_subdirs,
                    ctx.subdir_download_params(),
                    ctx.authentication_info(),
                    ctx.mirrors,
                    ctx.download_options(),
                    ctx.remote_fetch_params
                );
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

        std::vector<mamba::SubdirIndexLoader> m_subdirs;
        entry_list m_entries;
    };
}

void
bind_submodule_impl(pybind11::module_ m)
{
    using namespace mamba;

    /***************
     *  Migrators  *
     ***************/

    struct PackageInfoV2Migrator
    {
    };

    py::class_<PackageInfoV2Migrator>(m, "PackageInfo")
        .def(py::init(
            [](py::args, py::kwargs) -> PackageInfoV2Migrator
            {
                throw std::runtime_error(
                    "libmambapy.PackageInfo has been moved to libmambapy.specs.PackageInfo"
                );
            }
        ));

    struct MatchSpecV2Migrator
    {
    };

    py::class_<MatchSpecV2Migrator>(m, "MatchSpec")
        .def(py::init(
            [](py::args, py::kwargs) -> MatchSpecV2Migrator
            {
                // V2 migration
                throw std::runtime_error(
                    "libmambapy.MatchSpec has been moved to libmambapy.specs.MatchSpec"
                );
            }
        ));

    struct RepoV2Migrator
    {
    };

    py::class_<RepoV2Migrator>(m, "Repo").def(py::init(
        [](py::args, py::kwargs) -> RepoV2Migrator
        {
            throw std::runtime_error(  //
                "Use Pool.add_repo_from_repodata_json or Pool.add_repo_from_native_serialization"
                " instead and cache with Pool.native_serialize_repo."
                " Also consider load_subdir_in_database for a high_level function to load"
                " subdir index and manage cache, and load_installed_packages_in_database for"
                " loading prefix packages."
                " The Repo class itself has been moved to libmambapy.solver.libsolv.RepoInfo."
            );
        }
    ));

    struct PoolV2Migrator
    {
    };

    py::class_<PoolV2Migrator>(m, "Pool").def(py::init(
        [](py::args, py::kwargs) -> PoolV2Migrator
        {
            throw std::runtime_error(  //
                "libmambapy.Pool has been moved to libmambapy.solver.libsolv.Database."
                " The database contains functions to directly load packages, from a list or a"
                " repodata.json."
                " High level functions such as libmambapy.load_subdir_in_database and"
                " libmambapy.load_installed_packages_in_database are also available to work"
                " with other Mamba objects and Context parameters."
            );
        }
    ));

    constexpr auto global_solver_flag_v2_migrator = "V2 Migration: Solver flags set in libmambapy.solver.Request.flags.";
    m.attr("MAMBA_NO_DEPS") = global_solver_flag_v2_migrator;
    m.attr("MAMBA_ONLY_DEPS") = global_solver_flag_v2_migrator;
    m.attr("MAMBA_FORCE_REINSTALL") = global_solver_flag_v2_migrator;

    m.attr("SOLVER_FLAG_ALLOW_DOWNGRADE") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_ALLOW_ARCHCHANGE") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_ALLOW_VENDORCHANGE") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_ALLOW_UNINSTALL") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_NO_UPDATEPROVIDE") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_SPLITPROVIDES") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_IGNORE_RECOMMENDED") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_ADD_ALREADY_RECOMMENDED") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_NO_INFARCHCHECK") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_ALLOW_NAMECHANGE") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_KEEP_EXPLICIT_OBSOLETES") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_BEST_OBEY_POLICY") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_NO_AUTOTARGET") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_DUP_ALLOW_DOWNGRADE") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_DUP_ALLOW_ARCHCHANGE") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_DUP_ALLOW_VENDORCHANGE") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_DUP_ALLOW_NAMECHANGE") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_KEEP_ORPHANS") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_BREAK_ORPHANS") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_FOCUS_INSTALLED") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_YUM_OBSOLETES") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_NEED_UPDATEPROVIDE") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_URPM_REORDER") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_FOCUS_BEST") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_STRONG_RECOMMENDS") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_INSTALL_ALSO_UPDATES") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_ONLY_NAMESPACE_RECOMMENDED") = global_solver_flag_v2_migrator;
    m.attr("SOLVER_FLAG_STRICT_REPO_PRIORITY") = global_solver_flag_v2_migrator;


    constexpr auto global_solver_job_v2_migrator = "V2 Migration: job types are explicitly set in libmambapy.solver.Request.";
    m.attr("SOLVER_SOLVABLE") = global_solver_job_v2_migrator;
    m.attr("SOLVER_SOLVABLE_NAME") = global_solver_job_v2_migrator;
    m.attr("SOLVER_SOLVABLE_PROVIDES") = global_solver_job_v2_migrator;
    m.attr("SOLVER_SOLVABLE_ONE_OF") = global_solver_job_v2_migrator;
    m.attr("SOLVER_SOLVABLE_REPO") = global_solver_job_v2_migrator;
    m.attr("SOLVER_SOLVABLE_ALL") = global_solver_job_v2_migrator;
    m.attr("SOLVER_SELECTMASK") = global_solver_job_v2_migrator;
    m.attr("SOLVER_NOOP") = global_solver_job_v2_migrator;
    m.attr("SOLVER_INSTALL") = global_solver_job_v2_migrator;
    m.attr("SOLVER_ERASE") = global_solver_job_v2_migrator;
    m.attr("SOLVER_UPDATE") = global_solver_job_v2_migrator;
    m.attr("SOLVER_WEAKENDEPS") = global_solver_job_v2_migrator;
    m.attr("SOLVER_MULTIVERSION") = global_solver_job_v2_migrator;
    m.attr("SOLVER_LOCK") = global_solver_job_v2_migrator;
    m.attr("SOLVER_DISTUPGRADE") = global_solver_job_v2_migrator;
    m.attr("SOLVER_VERIFY") = global_solver_job_v2_migrator;
    m.attr("SOLVER_DROP_ORPHANED") = global_solver_job_v2_migrator;
    m.attr("SOLVER_USERINSTALLED") = global_solver_job_v2_migrator;
    m.attr("SOLVER_ALLOWUNINSTALL") = global_solver_job_v2_migrator;
    m.attr("SOLVER_FAVOR") = global_solver_job_v2_migrator;
    m.attr("SOLVER_DISFAVOR") = global_solver_job_v2_migrator;
    m.attr("SOLVER_JOBMASK") = global_solver_job_v2_migrator;
    m.attr("SOLVER_WEAK") = global_solver_job_v2_migrator;
    m.attr("SOLVER_ESSENTIAL") = global_solver_job_v2_migrator;
    m.attr("SOLVER_CLEANDEPS") = global_solver_job_v2_migrator;
    m.attr("SOLVER_ORUPDATE") = global_solver_job_v2_migrator;
    m.attr("SOLVER_FORCEBEST") = global_solver_job_v2_migrator;
    m.attr("SOLVER_TARGETED") = global_solver_job_v2_migrator;
    m.attr("SOLVER_NOTBYUSER") = global_solver_job_v2_migrator;
    m.attr("SOLVER_SETEV") = global_solver_job_v2_migrator;
    m.attr("SOLVER_SETEVR") = global_solver_job_v2_migrator;
    m.attr("SOLVER_SETARCH") = global_solver_job_v2_migrator;
    m.attr("SOLVER_SETVENDOR") = global_solver_job_v2_migrator;
    m.attr("SOLVER_SETREPO") = global_solver_job_v2_migrator;
    m.attr("SOLVER_NOAUTOSET") = global_solver_job_v2_migrator;
    m.attr("SOLVER_SETNAME") = global_solver_job_v2_migrator;
    m.attr("SOLVER_SETMASK") = global_solver_job_v2_migrator;

    enum struct SolverRuleinfoV2Migrator
    {
    };

    py::enum_<SolverRuleinfoV2Migrator>(m, "SolverRuleinfo")
        .def(py::init(
            [](py::args, py::kwargs) -> SolverRuleinfoV2Migrator
            {
                throw std::runtime_error("Direct access to libsolv objects is not longer supported.");
            }
        ));

    enum struct SolverV2Migrator
    {
    };

    py::enum_<SolverV2Migrator>(m, "Solver")
        .def(py::init(
            [](py::args, py::kwargs) -> SolverV2Migrator
            {
                throw std::runtime_error(
                    "libmambapy.Solver has been moved to libmambapy.solver.libsolv.Solver."
                );
            }
        ));

    /******************************************************
     *  Forward bindings to avoid C++ name in Python doc  *
     ******************************************************/

    // The lifetime of the unique Context instance will determine the lifetime of the other
    // singletons.
    using context_ptr = std::unique_ptr<Context, mambapy::destroy_singleton>;
    auto pyContext = py::class_<Context, context_ptr>(m, "Context");

    auto pyChannelContext = py::class_<ChannelContext>(m, "ChannelContext");

    py::register_exception<mamba_error>(m, "MambaNativeException");

    auto pyPrefixData = py::class_<PrefixData>(m, "PrefixData");

    auto pySubdirIndexLoader = py::class_<SubdirIndexLoader>(m, "SubdirIndexLoader");

    // only used in a return type; does it belong in the module?
    auto pyRootRole = py::class_<validation::RootRole>(m, "RootRole");

    auto pyOutputParams = py::class_<Context::OutputParams>(pyContext, "OutputParams");

    auto pyCommandParams = py::class_<CommandParams>(pyContext, "CommandParams");

    auto pyThreadsParams = py::class_<ThreadsParams>(pyContext, "ThreadsParams");

    auto pyPrefixParams = py::class_<PrefixParams>(pyContext, "PrefixParams");

    auto pyLinkParams = py::class_<LinkParams>(pyContext, "LinkParams");

    auto pyTransactionParams = py::class_<TransactionParams>(pyContext, "TransactionParams");

    auto pyValidationParams = py::class_<ValidationParams>(pyContext, "ValidationParams");

    /**************
     *  Bindings  *
     **************/

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

    py::add_ostream_redirect(m, "ostream_redirect");

    py::class_<download::RemoteFetchParams>(m, "RemoteFetchParams")
        .def(py::init<>())
        .def_readwrite("ssl_verify", &download::RemoteFetchParams::ssl_verify)
        .def_readwrite("max_retries", &download::RemoteFetchParams::max_retries)
        .def_readwrite("retry_timeout", &download::RemoteFetchParams::retry_timeout)
        .def_readwrite("retry_backoff", &download::RemoteFetchParams::retry_backoff)
        .def_readwrite("user_agent", &download::RemoteFetchParams::user_agent)
        // .def_readwrite("read_timeout_secs", &Context::RemoteFetchParams::read_timeout_secs)
        .def_readwrite("proxy_servers", &download::RemoteFetchParams::proxy_servers)
        .def_readwrite("connect_timeout_secs", &download::RemoteFetchParams::connect_timeout_secs);

    py::class_<download::Options>(m, "DownloadOptions")
        .def(py::init<>())
        .def_readwrite("download_threads", &download::Options::download_threads)
        .def_readwrite("fail_fast", &download::Options::fail_fast)
        .def_readwrite("sort", &download::Options::sort)
        .def_readwrite("verbose", &download::Options::verbose);

    py::class_<download::mirror_map>(m, "MirrorMap")
        .def(py::init<>())
        .def_static(
            "from_names_and_urls",
            [](py::iterable names_urls) -> download::mirror_map
            {
                auto map = download::mirror_map();
                for (py::handle mirror : names_urls)
                {
                    auto name = py::cast<std::string_view>(mirror[py::int_(0)]);
                    auto url = py::cast<std::string_view>(mirror[py::int_(1)]);
                    bool added = map.add_unique_mirror(name, download::make_mirror(std::string(url)));
                    if (!added)
                    {
                        throw mamba_error(
                            fmt::format(R"(Cannot add mirror "{}" with url "{}")", name, url),
                            mamba_error_code::incorrect_usage
                        );
                    }
                }
                return map;
            }
        )
        .def("has_mirrors", &download::mirror_map::has_mirrors, py::arg("mirror_name"))
        .def(
            "add_mirror_from_url",
            [](download::mirror_map& self, std::string_view mirror_name, std::string url) -> bool
            {
                return self.add_unique_mirror(
                    mirror_name,
                    download::make_mirror(url)

                );
            },
            py::arg("mirror_name"),
            py::arg("url")
        )
        .def("__contains__", &download::mirror_map::has_mirrors)
        .def("__len__", &download::mirror_map::size);

    m.def(
        "load_subdir_in_database",
        &load_subdir_in_database,
        py::arg("context"),
        py::arg("database"),
        py::arg("subdir")
    );

    m.def(
        "load_installed_packages_in_database",
        &load_installed_packages_in_database,
        py::arg("context"),
        py::arg("database"),
        py::arg("prefix_data")
    );

    py::class_<MultiPackageCache>(m, "MultiPackageCache")
        .def(
            py::init<>(
                [](Context& context, const std::vector<fs::u8path>& pkgs_dirs)
                {
                    deprecated(
                        "Use MultiPackageCache(pkgs_dirs, validation_params=context.validation_params) instead",
                        "2.2.1"
                    );
                    return MultiPackageCache{
                        pkgs_dirs,
                        context.validation_params,
                    };
                }
            ),
            py::arg("context"),
            py::arg("pkgs_dirs")
        )
        .def(
            py::init<const std::vector<fs::u8path>&, const ValidationParams&>(),
            py::arg("pkgs_dirs"),
            py::arg("validation_params")
        )
        .def("get_tarball_path", &MultiPackageCache::get_tarball_path)
        .def_property_readonly("first_writable_path", &MultiPackageCache::first_writable_path);

    py::class_<MTransaction>(m, "Transaction")
        .def(py::init<const Context&, solver::libsolv::Database&, const solver::Request&, solver::Solution, MultiPackageCache&>(
        ))
        .def("to_conda", &MTransaction::to_conda)
        .def("log_json", &MTransaction::log_json)
        .def("print", &MTransaction::print)
        .def("fetch_extract_packages", &MTransaction::fetch_extract_packages)
        .def("prompt", &MTransaction::prompt)
        .def("execute", &MTransaction::execute);

    py::class_<History>(m, "History")
        .def(
            py::init([](const fs::u8path& path, ChannelContext& channel_context)
                     { return History{ path, channel_context }; }),
            py::arg("path"),
            py::arg("channel_context")
        )
        .def("get_requested_specs_map", &History::get_requested_specs_map);

    static constexpr auto default_subdir_params = SubdirParams{};
    py::class_<SubdirParams>(m, "SubdirParams")
        .def(
            py::init(
                [](decltype(SubdirParams::local_repodata_ttl_s) local_repodata_ttl_s,
                   decltype(SubdirParams::offline) offline,
                   decltype(SubdirParams::repodata_force_use_zst) repodata_force_use_zst) -> SubdirParams
                {
                    return {
                        .local_repodata_ttl_s = std::move(local_repodata_ttl_s),
                        .offline = std::move(offline),
                        .repodata_force_use_zst = std::move(repodata_force_use_zst),
                    };
                }
            ),
            py::arg("local_repodata_ttl_s") = default_subdir_params.local_repodata_ttl_s,
            py::arg("offline") = default_subdir_params.offline,
            py::arg("repodata_force_use_zst") = default_subdir_params.repodata_force_use_zst
        )
        .def_readwrite("local_repodata_ttl_s", &SubdirParams::local_repodata_ttl_s)
        .def_readwrite("offline", &SubdirParams::offline)
        .def_readwrite("repodata_force_use_zst", &SubdirParams::repodata_force_use_zst);

    static constexpr auto default_subdir_download_params = SubdirDownloadParams{};
    py::class_<SubdirDownloadParams>(m, "SubdirDownloadParams")
        .def(
            py::init(
                [](decltype(SubdirDownloadParams::offline) offline,
                   decltype(SubdirDownloadParams::repodata_check_zst) repodata_check_zst
                ) -> SubdirDownloadParams
                {
                    return {
                        .offline = std::move(offline),
                        .repodata_check_zst = std::move(repodata_check_zst),
                    };
                }
            ),
            py::arg("offline") = default_subdir_download_params.offline,
            py::arg("repodata_check_zst") = default_subdir_download_params.repodata_check_zst
        )
        .def_readwrite("offline", &SubdirDownloadParams::offline)
        .def_readwrite("repodata_check_zst", &SubdirDownloadParams::repodata_check_zst);

    auto subdir_metadata = py::class_<SubdirMetadata>(m, "SubdirMetadata");

    static const auto default_http_metadata = SubdirMetadata::HttpMetadata{};
    py::class_<SubdirMetadata::HttpMetadata>(subdir_metadata, "HttpMetadata")
        .def(
            py::init(
                [](decltype(SubdirMetadata::HttpMetadata::url) url,
                   decltype(SubdirMetadata::HttpMetadata::etag) etag,
                   decltype(SubdirMetadata::HttpMetadata::last_modified) last_modified,
                   decltype(SubdirMetadata::HttpMetadata::cache_control) cache_control
                ) -> SubdirMetadata::HttpMetadata
                {
                    return {
                        .url = std::move(url),
                        .etag = std::move(etag),
                        .last_modified = std::move(last_modified),
                        .cache_control = std::move(cache_control),
                    };
                }
            ),
            py::arg("url") = default_http_metadata.url,
            py::arg("etag") = default_http_metadata.etag,
            py::arg("last_modified") = default_http_metadata.last_modified,
            py::arg("cache_control") = default_http_metadata.cache_control
        )
        .def_readwrite("url", &SubdirMetadata::HttpMetadata::url)
        .def_readwrite("etag", &SubdirMetadata::HttpMetadata::etag)
        .def_readwrite("last_modified", &SubdirMetadata::HttpMetadata::last_modified)
        .def_readwrite("cache_control", &SubdirMetadata::HttpMetadata::cache_control);

    subdir_metadata.def_static("read_state_file", &SubdirMetadata::read_state_file)
        .def_static("read_from_repodata_json", &SubdirMetadata::read_from_repodata_json)
        .def_static("read", &SubdirMetadata::read)
        .def("is_valid_metadata", &SubdirMetadata::is_valid_metadata)
        .def("url", &SubdirMetadata::url)
        .def("etag", &SubdirMetadata::etag)
        .def("last_modified", &SubdirMetadata::last_modified)
        .def("cache_control", &SubdirMetadata::cache_control)
        .def("has_up_to_date_zst", &SubdirMetadata::has_up_to_date_zst)
        .def("set_http_metadata", &SubdirMetadata::set_http_metadata)
        .def("set_zst", &SubdirMetadata::set_zst)
        .def("store_file_metadata", &SubdirMetadata::store_file_metadata)
        .def("write_state_file", &SubdirMetadata::write_state_file);

    pySubdirIndexLoader
        .def_static(
            "create",
            SubdirIndexLoader::create,
            py::arg("params"),
            py::arg("channel"),
            py::arg("platform"),
            py::arg("caches"),
            py::arg("repodata_filename") = "repodata.json"
        )
        .def_static(
            "download_required_indexes",
            [](py::iterable py_subdirs,
               const SubdirDownloadParams& subdir_download_params,
               const specs::AuthenticationDataBase& auth_info,
               const download::mirror_map& mirrors,
               const download::Options& download_options,
               const download::RemoteFetchParams& remote_fetch_params)
            {
                // TODO(C++23): Pass range to SubdirIndexLoader::create
                auto subdirs = std::vector<SubdirIndexLoader*>();
                subdirs.reserve(py::len_hint(py_subdirs));
                for (py::handle item : py_subdirs)
                {
                    subdirs.push_back(py::cast<SubdirIndexLoader*>(item));
                }
                return SubdirIndexLoader::download_required_indexes(
                    subdirs,
                    subdir_download_params,
                    auth_info,
                    mirrors,
                    download_options,
                    remote_fetch_params
                );
            },
            py::arg("subdir_indices"),
            py::arg("subdir_params"),
            py::arg("auth_info"),
            py::arg("mirrors"),
            py::arg("download_options"),
            py::arg("remote_fetch_params")
        )
        .def("is_noarch", &SubdirIndexLoader::is_noarch)
        .def("is_local", &SubdirIndexLoader::is_local)
        .def("channel", &SubdirIndexLoader::channel)
        .def("name", &SubdirIndexLoader::name)
        .def("channel_id", &SubdirIndexLoader::channel_id)
        .def("platform", &SubdirIndexLoader::platform)
        .def("metadata", &SubdirIndexLoader::metadata)
        .def("repodata_url", &SubdirIndexLoader::repodata_url)
        .def("caching_is_forbidden", &SubdirIndexLoader::caching_is_forbidden)
        .def("valid_cache_found", &SubdirIndexLoader::valid_cache_found)
        .def("valid_libsolv_cache_path", &SubdirIndexLoader::valid_libsolv_cache_path)
        .def("writable_libsolv_cache_path", &SubdirIndexLoader::writable_libsolv_cache_path)
        .def("valid_json_cache_path", &SubdirIndexLoader::valid_json_cache_path)
        .def("clear_valid_cache_files", &SubdirIndexLoader::clear_valid_cache_files);

    // Deprecated, replaced by SubdirIndexLoader in 2.3.0
    struct SubdirDataMigrator
    {
        mamba::SubdirIndexLoader* p_subdir_index;
    };

    // Deprecated, replaced by SubdirIndexLoader in 2.3.0
    py::class_<SubdirDataMigrator>(m, "SubdirData")
        .def(
            "create_repo",
            [](SubdirDataMigrator& self, Context& context, solver::libsolv::Database& database
            ) -> solver::libsolv::RepoInfo
            {
                deprecated("Use libmambapy.load_subdir_in_database instead", "2.0");
                return extract(load_subdir_in_database(context, database, *self.p_subdir_index));
            },
            py::arg("context"),
            py::arg("db")
        )
        .def(
            "loaded",
            [](const SubdirDataMigrator& self) { return self.p_subdir_index->valid_cache_found(); }
        )
        .def(
            "valid_solv_cache",
            // TODO make a proper well tested type caster for expected types.
            [](const SubdirDataMigrator& self) -> std::optional<fs::u8path>
            {
                if (auto f = self.p_subdir_index->valid_libsolv_cache_path())
                {
                    return { *std::move(f) };
                }
                return std::nullopt;
            }
        )
        .def(
            "valid_json_cache",
            [](const SubdirDataMigrator& self) -> std::optional<fs::u8path>
            {
                if (auto f = self.p_subdir_index->valid_json_cache_path())
                {
                    return { *std::move(f) };
                }
                return std::nullopt;
            }
        )
        .def(
            "cache_path",
            [](const SubdirDataMigrator& self) -> std::string
            {
                deprecated(
                    "Use `SubdirData.valid_solv_path` or `SubdirData.valid_json` path instead",
                    "2.0"
                );
                if (auto solv_path = self.p_subdir_index->valid_libsolv_cache_path())
                {
                    return solv_path->string();
                }
                else if (auto json_path = self.p_subdir_index->valid_json_cache_path())
                {
                    return json_path->string();
                }
                throw mamba_error("Cache not loaded", mamba_error_code::cache_not_loaded);
            }
        );

    using mambapy::SubdirIndex;
    using SubdirIndexEntry = SubdirIndex::Entry;

    // Deprecated, replaced by SubdirIndexLoader in 2.3.0
    py::class_<SubdirIndexEntry>(m, "SubdirIndexEntry")
        .def(py::init(
            []()
            {
                deprecated("Use SubdirIndexLoader", "2.3.0");
                return SubdirIndexEntry();
            }
        ))
        .def_property_readonly(
            "subdir",
            [](const SubdirIndexEntry& self) { return SubdirDataMigrator{ self.p_subdirdata }; },
            py::return_value_policy::reference
        )
        .def_readonly("platform", &SubdirIndexEntry::m_platform)
        .def_readonly("channel", &SubdirIndexEntry::p_channel, py::return_value_policy::reference)
        .def_readonly("url", &SubdirIndexEntry::m_url);

    py::class_<SubdirIndex>(m, "SubdirIndex")
        .def(py::init(
            []()
            {
                deprecated("Use SubdirIndexLoader", "2.3.0");
                return SubdirIndex();
            }
        ))
        .def(
            "create",
            [](SubdirIndex& self,
               Context& context,
               ChannelContext& channel_context,
               const specs::Channel& channel,
               const std::string& platform,
               const std::string& full_url,
               MultiPackageCache& caches,
               const std::string& repodata_fn,
               const std::string& url)
            {
                self.create(context, channel_context, channel, platform, full_url, caches, repodata_fn, url);
            },
            py::arg("context"),
            py::arg("channel_context"),
            py::arg("channel"),
            py::arg("platform"),
            py::arg("full_url"),
            py::arg("caches"),
            py::arg("repodata_fn"),
            py::arg("url")
        )
        .def("download", &SubdirIndex::download)
        .def("__len__", &SubdirIndex::size)
        .def("__getitem__", &SubdirIndex::operator[])
        .def(
            "__iter__",
            [](SubdirIndex& self) { return py::make_iterator(self.begin(), self.end()); },
            py::keep_alive<0, 1>()
        );

    m.def("cache_name_from_url", &cache_name_from_url);
    m.def(
        "cache_fn_url",
        [](std::string url)
        {
            deprecated("This function was renamed `cache_filename_from_url`.", "2.3.0");
            return cache_filename_from_url(std::move(url));
        }
    );
    m.def("cache_filename_from_url", &cache_filename_from_url);
    m.def("create_cache_dir", &create_cache_dir);

    py::enum_<ChannelPriority>(m, "ChannelPriority")
        .value("Flexible", ChannelPriority::Flexible)
        .value("Strict", ChannelPriority::Strict)
        .value("Disabled", ChannelPriority::Disabled);

    py::enum_<VerificationLevel>(m, "VerificationLevel")
        .value("Disabled", VerificationLevel::Disabled)
        .value("Warn", VerificationLevel::Warn)
        .value("Enabled", VerificationLevel::Enabled);

    py::enum_<mamba::log_level>(m, "LogLevel")
        .value("TRACE", mamba::log_level::trace)
        .value("DEBUG", mamba::log_level::debug)
        .value("INFO", mamba::log_level::info)
        .value("WARNING", mamba::log_level::warn)
        .value("ERROR", mamba::log_level::err)
        .value("CRITICAL", mamba::log_level::critical)
        .value("OFF", mamba::log_level::off);

    pyChannelContext.def_static("make_simple", &ChannelContext::make_simple)
        .def_static("make_conda_compatible", &ChannelContext::make_conda_compatible)
        .def(
            py::init<specs::ChannelResolveParams, std::vector<specs::Channel>>(),
            py::arg("params"),
            py::arg("has_zst")
        )
        .def("make_channel", py::overload_cast<std::string_view>(&ChannelContext::make_channel))
        .def("make_channel", py::overload_cast<specs::UnresolvedChannel>(&ChannelContext::make_channel))
        .def("params", &ChannelContext::params)
        .def("has_zst", &ChannelContext::has_zst);

    py::class_<Palette>(m, "Palette")
        .def_static("no_color", &Palette::no_color)
        .def_static("terminal", &Palette::terminal)
        .def_readwrite("success", &Palette::success)
        .def_readwrite("failure", &Palette::failure)
        .def_readwrite("external", &Palette::external)
        .def_readwrite("shown", &Palette::shown)
        .def_readwrite("safe", &Palette::safe)
        .def_readwrite("unsafe", &Palette::unsafe)
        .def_readwrite("user", &Palette::user)
        .def_readwrite("ignored", &Palette::ignored)
        .def_readwrite("addition", &Palette::addition)
        .def_readwrite("deletion", &Palette::deletion)
        .def_readwrite("progress_bar_none", &Palette::progress_bar_none)
        .def_readwrite("progress_bar_downloaded", &Palette::progress_bar_downloaded)
        .def_readwrite("progress_bar_extracted", &Palette::progress_bar_extracted);

    py::class_<Context::GraphicsParams>(m, "GraphicsParams")
        .def(py::init())
        .def_readwrite("no_progress_bars", &Context::GraphicsParams::no_progress_bars)
        .def_readwrite("palette", &Context::GraphicsParams::palette);


    py::class_<ContextOptions>(m, "ContextOptions")
        .def(
            py::init([](bool logging = mambapy::default_context_options.enable_logging,
                        bool signal_handling = mambapy::default_context_options.enable_signal_handling
                     ) { return ContextOptions{ logging, signal_handling }; }),
            py::arg("enable_logging") = true,
            py::arg("enable_signal_handling") = true
        )
        .def_readwrite("enable_logging", &ContextOptions::enable_logging)
        .def_readwrite("enable_signal_handling", &ContextOptions::enable_signal_handling)
        .def(
            "__repr__",
            [](const ContextOptions& self)
            {
                return fmt::format(
                    "ContextOptions(enable_logging={}, enable_signal_handling={})",
                    self.enable_logging,
                    self.enable_signal_handling
                );
            }
        );

    // The lifetime of the unique Context instance will determine the lifetime of the other
    // singletons.
    auto context_constructor = [](ContextOptions options = mambapy::default_context_options
                               ) -> context_ptr
    {
        if (mambapy::current_singletons)
        {
            throw std::runtime_error("Only one Context instance can exist at any time");
        }

        mambapy::current_singletons = std::make_unique<mambapy::Singletons>(options);
        assert(&mambapy::singletons() == mambapy::current_singletons.get());
        return context_ptr(&mambapy::singletons().context());
    };

    pyContext
        .def(py::init(context_constructor), py::arg("options") = mambapy::default_context_options)
        .def_static("use_default_signal_handler", &Context::use_default_signal_handler)
        .def_readwrite("graphics_params", &Context::graphics_params)
        .def_readwrite("offline", &Context::offline)
        .def_readwrite("local_repodata_ttl", &Context::local_repodata_ttl)
        .def_readwrite("use_index_cache", &Context::use_index_cache)
        .def_readwrite("always_yes", &Context::always_yes)
        .def_readwrite("show_anaconda_channel_warnings", &Context::show_anaconda_channel_warnings)
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
        .def_readwrite("experimental_repodata_parsing", &Context::experimental_repodata_parsing)
        .def_readwrite("solver_flags", &Context::solver_flags)
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

    pyContext.def_property_readonly_static(
        "RemoteFetchParams",
        [](py::handle) { return py::type::of<download::RemoteFetchParams>(); }
    );

    static const auto default_output_params = Context::OutputParams{};
    pyOutputParams
        .def(
            py::init(
                [](decltype(Context::OutputParams::verbosity) verbosity,
                   decltype(Context::OutputParams::json) json,
                   decltype(Context::OutputParams::quiet) quiet) -> Context::OutputParams
                {
                    return {
                        .verbosity = std::move(verbosity),
                        .json = std::move(json),
                        .quiet = std::move(quiet),
                    };
                }
            ),
            py::arg("verbosity") = default_output_params.verbosity,
            py::arg("json") = default_output_params.json,
            py::arg("quiet") = default_output_params.quiet
        )
        .def_readwrite("verbosity", &Context::OutputParams::verbosity)
        .def_readwrite("json", &Context::OutputParams::json)
        .def_readwrite("quiet", &Context::OutputParams::quiet);

    static constexpr auto default_threads_params = ThreadsParams{};
    pyThreadsParams
        .def(
            py::init(
                [](decltype(ThreadsParams::download_threads) download_threads,
                   decltype(ThreadsParams::extract_threads) extract_threads) -> ThreadsParams
                {
                    return {
                        .download_threads = std::move(download_threads),
                        .extract_threads = std::move(extract_threads),
                    };
                }
            ),
            py::arg("download_threads") = default_threads_params.download_threads,
            py::arg("extract_threads") = default_threads_params.extract_threads
        )
        .def_readwrite("download_threads", &ThreadsParams::download_threads)
        .def_readwrite("extract_threads", &ThreadsParams::extract_threads);

    static const auto default_command_params = CommandParams{};
    pyCommandParams
        .def(
            py::init(
                [](std::string caller_version,
                   std::string conda_version,
                   std::string current_command,
                   bool is_mamba_exe) -> CommandParams
                {
                    return {
                        .caller_version = std::move(caller_version),
                        .conda_version = std::move(conda_version),
                        .current_command = std::move(current_command),
                        .is_mamba_exe = is_mamba_exe,
                    };
                }
            ),
            py::arg("caller_version") = default_command_params.caller_version,
            py::arg("conda_version") = default_command_params.conda_version,
            py::arg("current_command") = default_command_params.current_command,
            py::arg("is_mamba_exe") = default_command_params.is_mamba_exe
        )
        .def_readwrite("caller_version", &CommandParams::caller_version)
        .def_readwrite("conda_version", &CommandParams::conda_version)
        .def_readwrite("current_command", &CommandParams::current_command)
        .def_readwrite("is_mamba_exe", &CommandParams::is_mamba_exe);

    static const auto default_prefix_params = PrefixParams{};
    pyPrefixParams
        .def(
            py::init(
                [](decltype(PrefixParams::target_prefix) target_prefix,
                   decltype(PrefixParams::conda_prefix) conda_prefix,
                   decltype(PrefixParams::root_prefix) root_prefix,
                   decltype(PrefixParams::relocate_prefix) relocate_prefix) -> PrefixParams
                {
                    return {
                        .target_prefix = std::move(target_prefix),
                        .root_prefix = std::move(root_prefix),
                        .conda_prefix = std::move(conda_prefix),
                        .relocate_prefix = std::move(relocate_prefix),
                    };
                }
            ),
            py::arg("target_prefix") = default_prefix_params.target_prefix,
            py::arg("conda_prefix") = default_prefix_params.conda_prefix,
            py::arg("root_prefix") = default_prefix_params.root_prefix,
            py::arg("relocate_prefix") = default_prefix_params.relocate_prefix
        )
        .def_readwrite("target_prefix", &PrefixParams::target_prefix)
        .def_readwrite("conda_prefix", &PrefixParams::conda_prefix)
        .def_readwrite("relocate_prefix", &PrefixParams::relocate_prefix)
        .def_readwrite("root_prefix", &PrefixParams::root_prefix);

    static constexpr auto default_link_params = LinkParams{};
    pyLinkParams
        .def(
            py::init(
                [](bool allow_softlinks, bool always_copy, bool always_softlink, bool compile_pyc
                ) -> LinkParams
                {
                    return {
                        .allow_softlinks = allow_softlinks,
                        .always_copy = always_copy,
                        .always_softlink = always_softlink,
                        .compile_pyc = compile_pyc,
                    };
                }
            ),
            py::arg("allow_softlinks") = default_link_params.allow_softlinks,
            py::arg("always_copy") = default_link_params.always_copy,
            py::arg("always_softlink") = default_link_params.always_softlink,
            py::arg("compile_pyc") = default_link_params.compile_pyc
        )
        .def_readwrite("allow_softlinks", &LinkParams::allow_softlinks)
        .def_readwrite("always_copy", &LinkParams::always_copy)
        .def_readwrite("always_softlink", &LinkParams::always_softlink)
        .def_readwrite("compile_pyc", &LinkParams::compile_pyc);

    static const auto default_validation_params = ValidationParams{};
    pyValidationParams
        .def(
            py::init(
                [](decltype(ValidationParams::safety_checks) safety_checks,
                   decltype(ValidationParams::extra_safety_checks) extra_safety_checks,
                   decltype(ValidationParams::verify_artifacts) verify_artifacts,
                   decltype(ValidationParams::trusted_channels) trusted_channels) -> ValidationParams
                {
                    return {
                        .safety_checks = std::move(safety_checks),
                        .extra_safety_checks = std::move(extra_safety_checks),
                        .verify_artifacts = std::move(verify_artifacts),
                        .trusted_channels = std::move(trusted_channels),
                    };
                }
            ),
            py::arg("safety_checks") = default_validation_params.safety_checks,
            py::arg("extra_safety_checks") = default_validation_params.extra_safety_checks,
            py::arg("verify_artifacts") = default_validation_params.verify_artifacts,
            py::arg("trusted_channels") = default_validation_params.trusted_channels
        )
        .def_readwrite("safety_checks", &ValidationParams::safety_checks)
        .def_readwrite("extra_safety_checks", &ValidationParams::extra_safety_checks)
        .def_readwrite("verify_artifacts", &ValidationParams::verify_artifacts)
        .def_readwrite("trusted_channels", &ValidationParams::trusted_channels);

    pyContext.def_readwrite("remote_fetch_params", &Context::remote_fetch_params)
        .def_readwrite("output_params", &Context::output_params)
        .def_readwrite("threads_params", &Context::threads_params)
        .def_readwrite("prefix_params", &Context::prefix_params)
        .def_readwrite("link_params", &Context::link_params)
        .def_readwrite("validation_params", &Context::validation_params);

    static const auto default_transaction_params = TransactionParams{};
    pyTransactionParams
        .def(
            py::init(
                [](bool is_mamba_exe,
                   bool json_output,
                   int verbosity,
                   bool shortcuts,
                   std::vector<fs::u8path> envs_dirs,
                   std::string platform,
                   PrefixParams prefix_params,
                   LinkParams link_params,
                   ThreadsParams threads_params) -> TransactionParams
                {
                    return {
                        .is_mamba_exe = is_mamba_exe,
                        .json_output = json_output,
                        .verbosity = verbosity,
                        .shortcuts = shortcuts,
                        .envs_dirs = std::move(envs_dirs),
                        .platform = std::move(platform),
                        .prefix_params = std::move(prefix_params),
                        .link_params = std::move(link_params),
                        .threads_params = std::move(threads_params),
                    };
                }
            ),
            py::arg("is_mamba_exe") = default_transaction_params.is_mamba_exe,
            py::arg("json_output") = default_transaction_params.json_output,
            py::arg("verbosity") = default_transaction_params.verbosity,
            py::arg("shortcuts") = default_transaction_params.shortcuts,
            py::arg("envs_dirs") = default_transaction_params.envs_dirs,
            py::arg("platform") = default_transaction_params.platform,
            py::arg("prefix_params") = default_transaction_params.prefix_params,
            py::arg("link_params") = default_transaction_params.link_params,
            py::arg("threads_params") = default_transaction_params.threads_params
        )
        .def_readwrite("is_mamba_exe", &TransactionParams::is_mamba_exe)
        .def_readwrite("json_output", &TransactionParams::json_output)
        .def_readwrite("verbosity", &TransactionParams::verbosity)
        .def_readwrite("shortcuts", &TransactionParams::shortcuts)
        .def_readwrite("envs_dirs", &TransactionParams::envs_dirs)
        .def_readwrite("platform", &TransactionParams::platform)
        .def_readwrite("prefix_params", &TransactionParams::prefix_params)
        .def_readwrite("link_params", &TransactionParams::link_params)
        .def_readwrite("threads_params", &TransactionParams::threads_params);

    ////////////////////////////////////////////
    //    Support the old deprecated API     ///
    ////////////////////////////////////////////
    // RemoteFetchParams
    pyContext
        .def_property(
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
    pyContext
        .def_property(
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
    pyContext
        .def_property(
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
    pyContext
        .def_property(
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

    // ValidationParams
    pyContext
        .def_property(
            "safety_checks",
            [](const Context& self)
            {
                deprecated("Use `validation_params.safety_checks` instead.");
                return self.validation_params.safety_checks;
            },
            [](Context& self, VerificationLevel sc)
            {
                deprecated("Use `validation_params.safety_checks` instead.");
                self.validation_params.safety_checks = sc;
            }
        )
        .def_property(
            "extra_safety_checks",
            [](const Context& self)
            {
                deprecated("Use `validation_params.extra_safety_checks` instead.");
                return self.validation_params.extra_safety_checks;
            },
            [](Context& self, bool esc)
            {
                deprecated("Use `validation_params.extra_safety_checks` instead.");
                self.validation_params.extra_safety_checks = esc;
            }
        )
        .def_property(
            "verify_artifacts",
            [](const Context& self)
            {
                deprecated("Use `validation_params.verify_artifacts` instead.");
                return self.validation_params.verify_artifacts;
            },
            [](Context& self, bool va)
            {
                deprecated("Use `validation_params.verify_artifacts` instead.");
                self.validation_params.verify_artifacts = va;
            }
        )
        .def_property(
            "trusted_channels",
            [](const Context& self)
            {
                deprecated("Use `validation_params.trusted_channels` instead.");
                return self.validation_params.trusted_channels;
            },
            [](Context& self, std::vector<std::string> tc)
            {
                deprecated("Use `validation_params.trusted_channels` instead.");
                self.validation_params.trusted_channels = tc;
            }
        );

    ////////////////////////////////////////////

    py::enum_<QueryType>(m, "QueryType")
        .value("Search", QueryType::Search)
        .value("Depends", QueryType::Depends)
        .value("WhoNeeds", QueryType::WhoNeeds)
        .def(py::init(&mambapy::enum_from_str<QueryType>))
        .def_static("parse", &query_type_parse);
    py::implicitly_convertible<py::str, QueryType>();

    py::enum_<QueryResultFormat>(m, "QueryResultFormat")
        .value("Json", QueryResultFormat::Json)
        .value("Tree", QueryResultFormat::Tree)
        .value("Table", QueryResultFormat::Table)
        .value("Pretty", QueryResultFormat::Pretty)
        .value("RecursiveTable", QueryResultFormat::RecursiveTable)
        .def(py::init(&mambapy::enum_from_str<QueryResultFormat>));
    py::implicitly_convertible<py::str, QueryType>();

    py::class_<QueryResult>(m, "QueryResult")
        .def_property_readonly("type", &QueryResult::type)
        .def_property_readonly("query", &QueryResult::query)
        .def("sort", &QueryResult::sort, py::return_value_policy::reference)
        .def("groupby", &QueryResult::groupby, py::return_value_policy::reference)
        .def("reset", &QueryResult::reset, py::return_value_policy::reference)
        .def("table", &QueryResult::table_to_str)
        .def("tree", &QueryResult::tree_to_str)
        .def("pretty", &QueryResult::pretty_to_str, py::arg("show_all_builds") = true)
        .def("json", [](const QueryResult& query) { return query.json().dump(); })
        .def(
            "to_dict",
            [](const QueryResult& query)
            {
                auto json_module = pybind11::module_::import("json");
                return json_module.attr("loads")(query.json().dump());
            }
        );

    py::class_<Query>(m, "Query")
        .def_static("find", &Query::find)
        .def_static("whoneeds", &Query::whoneeds)
        .def_static("depends", &Query::depends);

    pyPrefixData
        .def(
            py::init(
                [](const fs::u8path& prefix_path, ChannelContext& channel_context) -> PrefixData
                {
                    auto sres = PrefixData::create(prefix_path, channel_context);
                    if (sres.has_value())
                    {
                        return std::move(sres.value());
                    }
                    else
                    {
                        throw sres.error();
                    }
                }
            ),
            py::arg("path"),
            py::arg("channel_context")
        )
        .def_property_readonly("package_records", &PrefixData::records)
        .def("add_packages", &PrefixData::add_packages);

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

    py::class_<validation::v0_6::V06RoleBaseExtension, std::shared_ptr<validation::v0_6::V06RoleBaseExtension>>(
        m,
        "RoleBaseExtension"
    )
        .def_property_readonly("timestamp", &validation::v0_6::V06RoleBaseExtension::timestamp);

    py::class_<validation::v0_6::SpecImpl, validation::SpecBase, std::shared_ptr<validation::v0_6::SpecImpl>>(
        m,
        "SpecImpl"
    )
        .def(py::init<>());

    py::class_<
        validation::v0_6::KeyMgrRole,
        validation::RoleBase,
        validation::v0_6::V06RoleBaseExtension,
        std::shared_ptr<validation::v0_6::KeyMgrRole>>(m, "KeyMgr")
        .def(py::init<
             const std::string&,
             const validation::RoleFullKeys&,
             const std::shared_ptr<validation::SpecBase>>());

    py::class_<
        validation::v0_6::PkgMgrRole,
        validation::RoleBase,
        validation::v0_6::V06RoleBaseExtension,
        std::shared_ptr<validation::v0_6::PkgMgrRole>>(m, "PkgMgr")
        .def(py::init<
             const std::string&,
             const validation::RoleFullKeys&,
             const std::shared_ptr<validation::SpecBase>>());

    py::class_<
        validation::v0_6::RootImpl,
        validation::RoleBase,
        validation::v0_6::V06RoleBaseExtension,
        std::shared_ptr<validation::v0_6::RootImpl>>(m, "RootImpl")
        .def(py::init<const std::string&>(), py::arg("json_str"))
        .def(
            "update",
            [](validation::v0_6::RootImpl& role, const std::string& json_str)
            { return role.update(nlohmann::json::parse(json_str)); },
            py::arg("json_str")
        )
        .def(
            "create_key_mgr",
            [](validation::v0_6::RootImpl& role, const std::string& json_str)
            { return role.create_key_mgr(nlohmann::json::parse(json_str)); },
            py::arg("json_str")
        );

    m.def(
        "clean",
        [](Context&, int flags) { return clean(mambapy::singletons().config(), flags); },
        py::arg("context"),
        py::arg("flags")
    );

    m.def(
        "transmute",
        +[](Context& context,
            const fs::u8path& pkg_file,
            const fs::u8path& target,
            int compression_level,
            int compression_threads)
        {
            const auto extract_options = mamba::ExtractOptions::from_context(context);
            return transmute(pkg_file, target, compression_level, compression_threads, extract_options);
        },
        py::arg("context"),
        py::arg("source_package"),
        py::arg("destination_package"),
        py::arg("compression_level"),
        py::arg("compression_threads") = 1
    );

    m.def(
        "extract_package",
        [](const fs::u8path& file, const fs::u8path& destination, bool sparse)
        {
            return extract(
                file,
                destination,
                ExtractOptions{
                    /* .sparse= */ sparse,
                    // Unused by this function so we're not making it part of the API
                    /* .subproc_mode= */ extract_subproc_mode::mamba_package,
                }
            );
        }
    );


    m.def("init_console", &init_console);

    // fix extract from error_handling first
    // auto package_handling_sm = m.def_submodule("package_handling");
    // package_handling_sm.def("extract", &extract);
    // package_handling_sm.def("create", &create_package, py::arg("directory"),
    // py::arg("out_package"), py::arg("compression_level"), py::arg("compression_threads") = 1);


    m.def(
        "get_virtual_packages",
        [](Context& context) { return get_virtual_packages(context.platform); }
    );

    m.def("cancel_json_output", [](Context&) { mambapy::singletons().console().cancel_json_print(); });

    // CLEAN FLAGS
    m.attr("MAMBA_CLEAN_ALL") = MAMBA_CLEAN_ALL;
    m.attr("MAMBA_CLEAN_INDEX") = MAMBA_CLEAN_INDEX;
    m.attr("MAMBA_CLEAN_PKGS") = MAMBA_CLEAN_PKGS;
    m.attr("MAMBA_CLEAN_TARBALLS") = MAMBA_CLEAN_TARBALLS;
    m.attr("MAMBA_CLEAN_LOCKS") = MAMBA_CLEAN_LOCKS;
}

namespace mambapy
{
    void bind_submodule_legacy(pybind11::module_ m)
    {
        bind_submodule_impl(std::move(m));
    }
}
