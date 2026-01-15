// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <stdexcept>

#include "mamba/api/channel_loader.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/env_lockfile.hpp"
#include "mamba/core/environments_manager.hpp"
#include "mamba/core/history.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/pinning.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/download/downloader.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/solver/libsolv/solver.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"

#include "utils.hpp"

namespace mamba
{

    const auto& truthy_values(const std::string platform)
    {
        static std::unordered_map<std::string, bool> vals{
            { "win", false },
            { "unix", false },
            { "osx", false },
            { "linux", false },
        };

        if (util::starts_with(platform, "win"))
        {
            vals["win"] = true;
        }
        else
        {
            vals["unix"] = true;
            if (util::starts_with(platform, "linux"))
            {
                vals["linux"] = true;
            }
            else if (util::starts_with(platform, "osx"))
            {
                vals["osx"] = true;
            }
        }
        return vals;
    }

    namespace detail
    {
        bool eval_selector(const std::string& selector, const std::string& platform)
        {
            if (!(util::starts_with(selector, "sel(") && selector[selector.size() - 1] == ')'))
            {
                throw std::runtime_error(
                    "Couldn't parse selector. Needs to start with sel( and end with )"
                );
            }
            std::string expr = selector.substr(4, selector.size() - 5);

            const auto& values = truthy_values(platform);
            const auto found_it = values.find(expr);
            if (found_it == values.end())
            {
                throw std::runtime_error(
                    "Couldn't parse selector. Value not in [unix, linux, "
                    "osx, win] or additional whitespaces found."
                );
            }

            return found_it->second;
        }

        std::unique_ptr<TemporaryFile>
        downloaded_file_from_url(const Context& ctx, const std::string& url_str)
        {
            if (url_str.find("://") != std::string::npos)
            {
                LOG_INFO << "Downloading file from " << url_str;
                auto url_parts = util::rsplit(url_str, '/');
                std::string filename = (url_parts.size() == 1) ? "" : url_parts.back();
                auto tmp_file = std::make_unique<TemporaryFile>("mambaf", util::concat("_", filename));
                download::Request request(
                    "Environment lock or yaml file",
                    download::MirrorName(""),
                    url_str,
                    tmp_file->path()
                );
                const download::Result res = download::download(
                    std::move(request),
                    ctx.mirrors,
                    ctx.remote_fetch_params,
                    ctx.authentication_info(),
                    ctx.download_options()
                );

                if (!res || res.value().transfer.http_status != 200)
                {
                    throw std::runtime_error(
                        fmt::format("Could not download environment lock or yaml file from {}", url_str)
                    );
                }

                return tmp_file;
            }
            return nullptr;
        }

        yaml_file_contents read_yaml_file(
            const Context& ctx,
            const std::string& yaml_file,
            const std::string& platform,
            bool use_uv
        )
        {
            // Download content of environment yaml file
            auto tmp_yaml_file = downloaded_file_from_url(ctx, yaml_file);
            fs::u8path file;

            if (tmp_yaml_file)
            {
                file = tmp_yaml_file->path();
            }
            else
            {
                file = fs::weakly_canonical(util::expand_home(yaml_file));
                if (!fs::exists(file))
                {
                    LOG_ERROR << "YAML spec file '" << file.string() << "' not found";
                    throw std::runtime_error("File not found. Aborting.");
                }
            }

            yaml_file_contents result;
            YAML::Node f;
            try
            {
                f = YAML::LoadFile(file.string());
            }
            catch (YAML::Exception& e)
            {
                LOG_ERROR << "YAML error in spec file '" << file.string() << "'";
                throw e;
            }

            YAML::Node deps;
            if (f["dependencies"] && f["dependencies"].IsSequence() && f["dependencies"].size() > 0)
            {
                deps = f["dependencies"];
            }
            else
            {
                // Empty of absent `dependencies` key
                deps = YAML::Node(YAML::NodeType::Null);
            }
            YAML::Node final_deps;

            bool has_pip_deps = false;
            for (auto it = deps.begin(); it != deps.end(); ++it)
            {
                if (it->IsScalar())
                {
                    final_deps.push_back(*it);
                }
                else if (it->IsMap())
                {
                    // we merge a map to the upper level if the selector works
                    for (const auto& map_el : *it)
                    {
                        std::string key = map_el.first.as<std::string>();
                        if (util::starts_with(key, "sel("))
                        {
                            bool selected = detail::eval_selector(key, platform);
                            if (selected)
                            {
                                const YAML::Node& rest = map_el.second;
                                if (rest.IsScalar())
                                {
                                    final_deps.push_back(rest);
                                }
                                else
                                {
                                    throw std::runtime_error(
                                        "Complicated selection merge not implemented yet."
                                    );
                                }
                            }
                        }
                        else if (key == "pip")
                        {
                            std::string yaml_parent_path;
                            if (tmp_yaml_file)  // yaml file is fetched remotely
                            {
                                yaml_parent_path = yaml_file;
                            }
                            else
                            {
                                yaml_parent_path = fs::absolute(yaml_file).parent_path().string();
                            }
                            result.others_pkg_mgrs_specs.push_back(
                                {
                                    use_uv ? "uv" : "pip",
                                    map_el.second.as<std::vector<std::string>>(),
                                    yaml_parent_path,
                                }
                            );
                            has_pip_deps = true;
                        }
                    }
                }
            }

            std::vector<std::string> dependencies;
            try
            {
                if (final_deps.IsNull())
                {
                    dependencies = {};
                }
                else
                {
                    dependencies = final_deps.as<std::vector<std::string>>();
                }
            }
            catch (const YAML::Exception& e)
            {
                LOG_ERROR << "Bad conversion of 'dependencies' to a vector of string: " << final_deps;
                throw e;
            }

            if (has_pip_deps && use_uv && !std::count(dependencies.begin(), dependencies.end(), "uv"))
            {
                dependencies.push_back("uv");
            }
            else if (has_pip_deps && std::count(dependencies.begin(), dependencies.end(), "uv"))
            {
                for (auto& spec : result.others_pkg_mgrs_specs)
                {
                    if (spec.pkg_mgr == "pip")
                    {
                        spec.pkg_mgr = "uv";
                    }
                }
            }
            else if (has_pip_deps && !std::count(dependencies.begin(), dependencies.end(), "pip"))
            {
                dependencies.push_back("pip");
            }

            result.dependencies = dependencies;

            if (f["channels"])
            {
                try
                {
                    result.channels = f["channels"].as<std::vector<std::string>>();
                }
                catch (YAML::Exception& e)
                {
                    LOG_ERROR << "Could not read 'channels' as vector of strings from '"
                              << file.string() << "'";
                    throw e;
                }
            }
            else
            {
                LOG_DEBUG << "No 'channels' specified in YAML spec file '" << file.string() << "'";
            }

            if (f["name"])
            {
                result.name = f["name"].as<std::string>();
            }
            else
            {
                LOG_DEBUG << "No env 'name' specified in YAML spec file '" << file.string() << "'";
            }

            if (f["variables"])
            {
                result.variables = f["variables"].as<std::map<std::string, std::string>>();
            }
            else
            {
                LOG_DEBUG << "No 'variables' specified in YAML spec file '" << file.string() << "'";
            }

            return result;
        }

        bool operator==(const other_pkg_mgr_spec& s1, const other_pkg_mgr_spec& s2)
        {
            return (s1.pkg_mgr == s2.pkg_mgr) && (s1.deps == s2.deps) && (s1.cwd == s2.cwd);
        }
    }

    void install_revision(Configuration& config, std::size_t revision)
    {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("use_default_prefix_fallback").set_value(true);
        config.at("use_root_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(
                MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_NOT_ALLOW_MISSING_PREFIX
                | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_EXPECT_EXISTING_PREFIX
            );
        config.load();

        auto& context = config.context();
        auto channel_context = ChannelContext::make_conda_compatible(context);

        detail::install_revision(context, channel_context, revision);
    }

    void install(Configuration& config)
    {
        auto& ctx = config.context();

        config.at("create_base").set_value(true);
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("use_default_prefix_fallback").set_value(true);
        config.at("use_root_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(
                MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_NOT_ALLOW_MISSING_PREFIX
                | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_EXPECT_EXISTING_PREFIX
            );
        config.load();

        auto& install_specs = config.at("specs").value<std::vector<std::string>>();
        auto& use_explicit = config.at("explicit_install").value<bool>();

        auto& context = config.context();
        auto channel_context = ChannelContext::make_conda_compatible(context);

        if (context.env_lockfile)
        {
            const auto lockfile_path = context.env_lockfile.value();
            LOG_DEBUG << "Lockfile: " << lockfile_path;
            install_lockfile_specs(
                ctx,
                channel_context,
                lockfile_path,
                config.at("categories").value<std::vector<std::string>>(),
                false
            );
        }
        else if (!install_specs.empty())
        {
            if (use_explicit)
            {
                install_explicit_specs(ctx, channel_context, install_specs, false);
            }
            else
            {
                mamba::install_specs(context, channel_context, config, install_specs, false);
            }
        }
        else
        {
            Console::instance().print("Nothing to do.");
        }
    }

    struct InstallRequestOptions
    {
        bool freeze_installed = false;
        bool prefix_data_interoperability = false;
    };

    auto create_install_request(
        PrefixData& prefix_data,
        std::vector<std::string> specs,
        const InstallRequestOptions options = {}
    ) -> solver::Request
    {
        using Request = solver::Request;

        const auto& prefix_pkgs = prefix_data.records();

        auto request = Request();
        request.jobs.reserve(
            specs.size() + static_cast<size_t>(options.freeze_installed) * prefix_pkgs.size()
        );

        // Consider if a FreezeAll type in Request is relevant?
        if (options.freeze_installed && !prefix_pkgs.empty())
        {
            LOG_INFO << "Locking environment: " << prefix_pkgs.size() << " packages freezed";
            for (const auto& [name, pkg] : prefix_pkgs)
            {
                request.jobs.emplace_back(
                    Request::Freeze{
                        specs::MatchSpec::parse(name)
                            .or_else([](specs::ParseError&& err) { throw std::move(err); })
                            .value(),
                    }
                );
            }
        }

        // When prefix_data_interoperability is enabled, use Update requests instead of Install
        // for packages that conflict with pip packages. This tells the solver to replace the
        // pip package with the conda version.
        if (options.prefix_data_interoperability)
        {
            const auto& pip_pkgs = prefix_data.pip_records();
            for (const auto& s : specs)
            {
                auto spec = specs::MatchSpec::parse(s)
                                .or_else([](specs::ParseError&& err) { throw std::move(err); })
                                .value();

                // Check if there's a pip package with the same name
                if (spec.name().is_exact())
                {
                    const auto spec_name_str = spec.name().to_string();
                    if (pip_pkgs.find(spec_name_str) != pip_pkgs.end())
                    {
                        // Use Update instead of Install to replace the pip package
                        request.jobs.emplace_back(
                            Request::Update{
                                std::move(spec),
                                /* .clean_dependencies= */ false,
                            }
                        );
                        continue;
                    }
                }
                // No pip package conflict, use normal Install
                request.jobs.emplace_back(
                    Request::Install{
                        std::move(spec),
                    }
                );
            }
        }
        else
        {
            // Interoperability disabled, use normal Install requests
            for (const auto& s : specs)
            {
                request.jobs.emplace_back(
                    Request::Install{
                        specs::MatchSpec::parse(s)
                            .or_else([](specs::ParseError&& err) { throw std::move(err); })
                            .value(),
                    }
                );
            }
        }
        return request;
    }

    void add_pins_to_request(
        solver::Request& request,
        const Context& ctx,
        PrefixData& prefix_data,
        std::vector<std::string> specs,
        bool no_pin,
        bool no_py_pin
    )
    {
        using Request = solver::Request;

        const auto estimated_jobs_count = request.jobs.size()
                                          + (!no_pin) * ctx.pinned_packages.size() + !no_py_pin;
        request.jobs.reserve(estimated_jobs_count);
        if (!no_pin)
        {
            for (const auto& pin : file_pins(prefix_data.path() / "conda-meta" / "pinned"))
            {
                request.jobs.emplace_back(
                    Request::Pin{
                        specs::MatchSpec::parse(pin)
                            .or_else([](specs::ParseError&& err) { throw std::move(err); })
                            .value(),
                    }
                );
            }
            for (const auto& pin : ctx.pinned_packages)
            {
                request.jobs.emplace_back(
                    Request::Pin{
                        specs::MatchSpec::parse(pin)
                            .or_else([](specs::ParseError&& err) { throw std::move(err); })
                            .value(),
                    }
                );
            }
        }

        if (!no_py_pin)
        {
            auto py_pins = python_pin(prefix_data, specs);
            for (const auto& py_pin : py_pins)
            {
                request.jobs.emplace_back(
                    Request::Pin{
                        specs::MatchSpec::parse(py_pin)
                            .or_else([](specs::ParseError&& err) { throw std::move(err); })
                            .value(),
                    }
                );
            }
        }
    }

    void print_request_pins_to(const solver::Request& request, std::ostream& out)
    {
        for (const auto& req : request.jobs)
        {
            bool first = true;
            std::visit(
                [&](const auto& item)
                {
                    if constexpr (std::is_same_v<std::decay_t<decltype(item)>, solver::Request::Pin>)
                    {
                        if (first)
                        {
                            out << "\nPinned packages:\n\n";
                            first = false;
                        }
                        out << "  - " << item.spec.to_string() << '\n';
                    }
                },
                req
            );
        }
    }

    namespace
    {
        void print_activation_message(const Context& ctx)
        {
            // Check that the target prefix is not active before printing the activation message
            if (util::get_env("CONDA_PREFIX") != ctx.prefix_params.target_prefix)
            {
                // Get the name of the executable used directly from the command.
                const auto executable = get_self_exe_path().stem().string();

                // Get the name of the environment
                const auto environment = env_name(
                    ctx.envs_dirs,
                    ctx.prefix_params.root_prefix,
                    ctx.prefix_params.target_prefix
                );

                Console::stream() << "\nTo activate this environment, use:\n\n"
                                     "    "
                                  << executable << " activate " << environment
                                  << "\n\n"
                                     "Or to execute a single command in this environment, use:\n\n"
                                     "    "
                                  << executable
                                  << " run "
                                  // Use -n or -p depending on if the env_name is a full prefix or
                                  // just a name.
                                  << (environment == ctx.prefix_params.target_prefix ? "-p " : "-n ")
                                  << environment << " mycommand\n";
            }
        }

        void install_specs_impl(
            Context& ctx,
            ChannelContext& channel_context,
            const Configuration& config,
            const std::vector<std::string>& raw_specs,
            bool create_env,
            bool remove_prefix_on_failure,
            bool is_retry
        )
        {
            assert(&config.context() == &ctx);

            auto& no_pin = config.at("no_pin").value<bool>();
            auto& no_py_pin = config.at("no_py_pin").value<bool>();
            auto& freeze_installed = config.at("freeze_installed").value<bool>();
            auto& retry_clean_cache = config.at("retry_clean_cache").value<bool>();
            auto& env_vars = config.at("spec_file_env_vars").value<std::map<std::string, std::string>>();
            auto& no_env = config.at("no_env").value<bool>();

            if (ctx.prefix_params.target_prefix.empty())
            {
                throw std::runtime_error("No active target prefix");
            }
            if (!fs::exists(ctx.prefix_params.target_prefix) && create_env == false)
            {
                throw std::runtime_error(
                    fmt::format("Prefix does not exist at: {}", ctx.prefix_params.target_prefix.string())
                );
            }

            MultiPackageCache package_caches{ ctx.pkgs_dirs, ctx.validation_params };

            populate_context_channels_from_specs(raw_specs, ctx);

            if (ctx.channels.empty() && !ctx.offline)
            {
                LOG_WARNING << "No 'channels' specified";
            }

            solver::libsolv::Database db{
                channel_context.params(),
                {
                    ctx.experimental_matchspec_parsing ? solver::libsolv::MatchSpecParser::Mamba
                                                       : solver::libsolv::MatchSpecParser::Libsolv,
                },
            };
            add_logger_to_database(db);

            auto maybe_load = load_channels(ctx, channel_context, db, package_caches);
            if (!maybe_load)
            {
                throw std::runtime_error(maybe_load.error().what());
            }

            auto maybe_prefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
            if (!maybe_prefix_data)
            {
                throw std::runtime_error(maybe_prefix_data.error().what());
            }
            PrefixData& prefix_data = maybe_prefix_data.value();

            load_installed_packages_in_database(ctx, db, prefix_data);


            auto request = create_install_request(
                prefix_data,
                raw_specs,
                InstallRequestOptions{ .freeze_installed = freeze_installed,
                                       .prefix_data_interoperability = ctx.prefix_data_interoperability }
            );
            add_pins_to_request(request, ctx, prefix_data, raw_specs, no_pin, no_py_pin);
            request.flags = ctx.solver_flags;

            {
                auto out = Console::stream();
                print_request_pins_to(request, out);
                // Console stream prints on destruction
            }

            auto outcome = solver::libsolv::Solver()
                               .solve(
                                   db,
                                   request,
                                   ctx.experimental_matchspec_parsing
                                       ? solver::libsolv::MatchSpecParser::Mamba
                                       : solver::libsolv::MatchSpecParser::Mixed
                               )
                               .value();

            if (auto* unsolvable = std::get_if<solver::libsolv::UnSolvable>(&outcome))
            {
                unsolvable->explain_problems_to(
                    db,
                    LOG_ERROR,
                    {
                        /* .unavailable= */ ctx.graphics_params.palette.failure,
                        /* .available= */ ctx.graphics_params.palette.success,
                    }
                );
                if (retry_clean_cache && !is_retry)
                {
                    ctx.local_repodata_ttl = 2;
                    bool retry = true;
                    return install_specs_impl(
                        ctx,
                        channel_context,
                        config,
                        raw_specs,
                        create_env,
                        remove_prefix_on_failure,
                        retry
                    );
                }
                if (freeze_installed)
                {
                    Console::instance().print("Possible hints:\n  - 'freeze_installed' is turned on\n");
                }

                if (ctx.output_params.json)
                {
                    Console::instance().json_write(
                        { { "success", false }, { "solver_problems", unsolvable->problems(db) } }
                    );
                }
                throw mamba_error(
                    "Could not solve for environment specs",
                    mamba_error_code::satisfiablitity_error
                );
            }

            std::vector<LockFile> locks;

            for (auto& c : ctx.pkgs_dirs)
            {
                locks.push_back(LockFile(c));
            }

            Console::instance().json_write({ { "success", true } });

            // The point here is to delete the database before executing the transaction.
            // The database can have high memory impact, since installing packages
            // requires downloading, extracting, and launching Python interpreters for
            // creating ``.pyc`` files.
            // Ideally this whole function should be properly refactored and the transaction itself
            // should not need the database.
            auto trans = [&](auto database)
            {
                return MTransaction(  //
                    ctx,
                    database,
                    request,
                    std::get<solver::Solution>(outcome),
                    package_caches
                );
            }(std::move(db));

            if (ctx.output_params.json)
            {
                trans.log_json();
            }

            Console::stream();

            if (trans.prompt(ctx, channel_context))
            {
                if (create_env && !ctx.dry_run)
                {
                    detail::create_target_directory(ctx, ctx.prefix_params.target_prefix);
                }

                detail::populate_state_file(ctx.prefix_params.target_prefix, env_vars, no_env);

                trans.execute(ctx, channel_context, prefix_data);

                // Print activation message only if the environment is freshly created
                if (create_env)
                {
                    print_activation_message(ctx);
                }

                if (!ctx.dry_run)
                {
                    for (auto other_spec : config.at("others_pkg_mgrs_specs")
                                               .value<std::vector<detail::other_pkg_mgr_spec>>())
                    {
                        auto result = install_for_other_pkgmgr(ctx, other_spec, pip::Update::No);
                        if (!result)
                        {
                            static_assert(
                                std::is_base_of_v<std::exception, decltype(result)::error_type>
                            );
                            throw std::move(result).error();
                        }
                    }
                }
            }
            else
            {
                // Aborting new env creation
                // but the directory was already created because of `store_platform_config` call
                // => Remove the created directory
                if (remove_prefix_on_failure && fs::is_directory(ctx.prefix_params.target_prefix))
                {
                    fs::remove_all(ctx.prefix_params.target_prefix);
                }
            }
        }
    }

    void install_specs(
        Context& ctx,
        ChannelContext& channel_context,
        const Configuration& config,
        const std::vector<std::string>& specs,
        bool create_env,
        bool remove_prefix_on_failure
    )
    {
        auto is_retry = false;
        return install_specs_impl(
            ctx,
            channel_context,
            config,
            specs,
            create_env,
            remove_prefix_on_failure,
            is_retry
        );
    }

    namespace
    {

        // TransactionFunc: (Database& database, MultiPackageCache& package_caches) -> MTransaction
        template <typename TransactionFunc>
        void install_explicit_with_transaction(
            Context& ctx,
            ChannelContext& channel_context,
            const std::vector<std::string>& specs,
            TransactionFunc create_transaction,
            bool create_env,
            bool remove_prefix_on_failure
        )
        {
            solver::libsolv::Database database{
                channel_context.params(),
                {
                    ctx.experimental_matchspec_parsing ? solver::libsolv::MatchSpecParser::Mamba
                                                       : solver::libsolv::MatchSpecParser::Libsolv,
                },
            };
            add_logger_to_database(database);

            init_channels(ctx, channel_context);
            // Some use cases provide a list of explicit specs, but an empty
            // context. We need to create channels from the specs to be able
            // to download packages.
            init_channels_from_package_urls(ctx, channel_context, specs);
            auto maybe_prefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
            if (!maybe_prefix_data)
            {
                // TODO: propagate tl::expected mechanism
                throw std::runtime_error(
                    fmt::format("could not load prefix data: {}", maybe_prefix_data.error().what())
                );
            }
            PrefixData& prefix_data = maybe_prefix_data.value();

            MultiPackageCache pkg_caches(ctx.pkgs_dirs, ctx.validation_params);

            load_installed_packages_in_database(ctx, database, prefix_data);

            std::vector<detail::other_pkg_mgr_spec> others;
            // Note that the Transaction will gather the Solvables,
            // so they must have been ready in the database's pool before this line
            auto transaction = create_transaction(database, pkg_caches, others);

            std::vector<LockFile> lock_pkgs;

            for (auto& c : ctx.pkgs_dirs)
            {
                lock_pkgs.push_back(LockFile(c));
            }

            if (ctx.output_params.json)
            {
                transaction.log_json();
            }

            if (transaction.prompt(ctx, channel_context))
            {
                if (create_env && !ctx.dry_run)
                {
                    detail::create_target_directory(ctx, ctx.prefix_params.target_prefix);
                }

                transaction.execute(ctx, channel_context, prefix_data);

                // Print activation message only if the environment is freshly created
                if (create_env)
                {
                    print_activation_message(ctx);
                }

                for (auto other_spec : others)
                {
                    auto result = install_for_other_pkgmgr(ctx, other_spec, pip::Update::No);
                    if (!result.has_value())
                    {
                        static_assert(std::is_base_of_v<std::exception, decltype(result)::error_type>);
                        throw std::move(result).error();
                    }
                }
            }
            else
            {
                // Aborting new env creation
                // but the directory was already created because of `store_platform_config` call
                // => Remove the created directory
                if (remove_prefix_on_failure && fs::is_directory(ctx.prefix_params.target_prefix))
                {
                    fs::remove_all(ctx.prefix_params.target_prefix);
                }
            }
        }
    }

    void install_explicit_specs(
        Context& ctx,
        ChannelContext& channel_context,
        const std::vector<std::string>& specs,
        bool create_env,
        bool remove_prefix_on_failure
    )
    {
        install_explicit_with_transaction(
            ctx,
            channel_context,
            specs,
            [&](auto& db, auto& pkg_caches, auto& others)
            { return create_explicit_transaction_from_urls(ctx, db, specs, pkg_caches, others); },
            create_env,
            remove_prefix_on_failure
        );
    }

    void install_lockfile_specs(
        Context& ctx,
        ChannelContext& channel_context,
        const std::string& lockfile,
        const std::vector<std::string>& categories,
        bool create_env,
        bool remove_prefix_on_failure
    )
    {
        fs::u8path file;
        auto tmp_lock_file = detail::downloaded_file_from_url(ctx, lockfile);

        if (tmp_lock_file)
        {
            file = tmp_lock_file->path();
        }
        else
        {
            file = lockfile;
        }

        install_explicit_with_transaction(
            ctx,
            channel_context,
            {},
            [&](auto& db, auto& pkg_caches, auto& others)
            {
                return create_explicit_transaction_from_lockfile(
                    ctx,
                    db,
                    file,
                    categories,
                    pkg_caches,
                    others
                );
            },
            create_env,
            remove_prefix_on_failure
        );
    }

    namespace detail
    {
        enum SpecType
        {
            unknown,
            env_lockfile,
            yaml,
            other
        };

        void create_empty_target(
            const Context& context,
            const fs::u8path& prefix,
            const std::map<std::string, std::string>& env_vars,
            bool no_env
        )
        {
            detail::create_target_directory(context, prefix);

            populate_state_file(prefix, env_vars, no_env);

            Console::instance().print(
                util::join(
                    "",
                    std::vector<std::string>({ "Empty environment created at prefix: ",
                                               prefix.string() })
                )
            );
            Console::instance().json_write({ { "success", true } });
        }

        void populate_state_file(
            const fs::u8path& prefix,
            const std::map<std::string, std::string>& env_vars,
            bool no_env
        )
        {
            if (!env_vars.empty())
            {
                if (!no_env)
                {
                    const fs::u8path env_vars_file_path = prefix / "conda-meta" / "state";

                    // Read existing state file to preserve other fields
                    nlohmann::json j;
                    if (fs::exists(env_vars_file_path))
                    {
                        auto fin = open_ifstream(env_vars_file_path);
                        try
                        {
                            fin >> j;
                        }
                        catch (nlohmann::json::exception&)
                        {
                            // If parsing fails, start with empty JSON
                            j = nlohmann::json::object();
                        }
                    }

                    // Merge new env_vars with existing ones
                    if (!j.contains("env_vars") || !j["env_vars"].is_object())
                    {
                        j["env_vars"] = nlohmann::json::object();
                    }
                    for (const auto& [key, value] : env_vars)
                    {
                        j["env_vars"][util::to_upper(key)] = value;
                    }

                    // Write back
                    fs::create_directories(env_vars_file_path.parent_path());
                    std::ofstream out = open_ofstream(env_vars_file_path);
                    if (out.fail())
                    {
                        throw std::runtime_error("Couldn't open file: " + env_vars_file_path.string());
                    }
                    out << j.dump(4);
                }
                else
                {
                    LOG_WARNING << "Using `no-env`. Variables from yaml file are not considered.";
                }
            }
        }

        void create_target_directory(const Context& context, const fs::u8path prefix)
        {
            path::touch(prefix / "conda-meta" / "history", true);

            // Register the environment
            EnvironmentsManager env_manager{ context };
            env_manager.register_env(prefix);
        }

        void file_specs_hook(Configuration& config, std::vector<std::string>& file_specs)
        {
            auto& env_name = config.at("spec_file_env_name");
            auto& specs = config.at("specs");
            auto& others_pkg_mgrs_specs = config.at("others_pkg_mgrs_specs");
            auto& channels = config.at("channels");
            auto& env_vars = config.at("spec_file_env_vars");

            auto& context = config.context();

            if (file_specs.size() == 0)
            {
                return;
            }

            mamba::detail::SpecType spec_type = mamba::detail::unknown;
            for (auto& file : file_specs)
            {
                mamba::detail::SpecType current_file_spec_type = mamba::detail::unknown;
                if (is_env_lockfile_name(file))
                {
                    current_file_spec_type = mamba::detail::env_lockfile;
                }
                else if (is_yaml_file_name(file))
                {
                    current_file_spec_type = mamba::detail::yaml;
                }
                else
                {
                    current_file_spec_type = mamba::detail::other;
                }

                if (spec_type != mamba::detail::unknown && spec_type != current_file_spec_type)
                {
                    throw std::runtime_error(
                        "found multiple spec file types, all spec files must be of same format (yaml, txt, explicit spec, etc.)"
                    );
                }

                spec_type = current_file_spec_type;
            }

            for (auto& file : file_specs)
            {
                // read specs from file :)
                if (is_env_lockfile_name(file))
                {
                    if (util::starts_with(file, "http"))
                    {
                        context.env_lockfile = file;
                    }
                    else
                    {
                        context.env_lockfile = fs::absolute(file).string();
                    }

                    LOG_DEBUG << "File spec Lockfile: " << context.env_lockfile.value();
                }
                else if (is_yaml_file_name(file))
                {
                    const auto parse_result = read_yaml_file(
                        context,
                        file,
                        context.platform,
                        context.use_uv
                    );

                    if (parse_result.channels.size() != 0)
                    {
                        std::vector<std::string> updated_channels;
                        if (channels.cli_configured())
                        {
                            updated_channels = channels.cli_value<std::vector<std::string>>();
                        }
                        for (auto& c : parse_result.channels)
                        {
                            // Substitute env vars in channels from env yaml file,
                            // before pushing them to the global list of channels
                            updated_channels.push_back(expandvars(c));
                        }
                        channels.set_cli_value(updated_channels);
                    }

                    if (parse_result.name.size() != 0 && !env_name.configured())
                    {
                        env_name.set_cli_yaml_value(parse_result.name);
                    }
                    else if (parse_result.name.size() != 0
                             && parse_result.name != env_name.cli_value<std::string>())
                    {
                        LOG_WARNING << "YAML specs have different environment names. Using "
                                    << env_name.cli_value<std::string>();
                    }

                    if (parse_result.dependencies.size() != 0)
                    {
                        std::vector<std::string> updated_specs;
                        if (specs.cli_configured())
                        {
                            updated_specs = specs.cli_value<std::vector<std::string>>();
                        }
                        for (auto& s : parse_result.dependencies)
                        {
                            updated_specs.push_back(s);
                        }
                        specs.set_cli_value(updated_specs);
                    }

                    if (parse_result.others_pkg_mgrs_specs.size() != 0)
                    {
                        std::vector<mamba::detail::other_pkg_mgr_spec> updated_specs;
                        if (others_pkg_mgrs_specs.cli_configured())
                        {
                            updated_specs = others_pkg_mgrs_specs.cli_value<
                                std::vector<mamba::detail::other_pkg_mgr_spec>>();
                        }
                        for (auto& s : parse_result.others_pkg_mgrs_specs)
                        {
                            updated_specs.push_back(s);
                        }
                        others_pkg_mgrs_specs.set_cli_value(updated_specs);
                    }

                    if (parse_result.variables.size() != 0)
                    {
                        std::map<std::string, std::string> updated_env_vars;
                        if (env_vars.cli_configured())
                        {
                            updated_env_vars = env_vars.cli_value<std::map<std::string, std::string>>();
                        }
                        updated_env_vars.insert(
                            parse_result.variables.cbegin(),
                            parse_result.variables.cend()
                        );
                        env_vars.set_cli_value(updated_env_vars);
                    }
                }
                else
                {
                    const std::vector<std::string> file_contents = read_lines(file);
                    if (file_contents.size() == 0)
                    {
                        throw std::runtime_error(util::concat("Got an empty file: ", file));
                    }

                    // Inferring potential explicit environment specification
                    for (std::size_t i = 0; i < file_contents.size(); ++i)
                    {
                        auto& line = file_contents[i];
                        if (util::starts_with(line, "@EXPLICIT"))
                        {
                            // this is an explicit env
                            // we can check if the platform is correct with the previous line
                            std::string platform;
                            if (i >= 1)
                            {
                                for (std::size_t j = 0; j < i; ++j)
                                {
                                    platform = file_contents[j];
                                    if (util::starts_with(platform, "# platform: "))
                                    {
                                        platform = platform.substr(12);
                                        break;
                                    }
                                }
                            }
                            LOG_INFO << "Installing explicit specs for platform " << platform;

                            std::vector<std::string> explicit_specs;
                            for (auto f = file_contents.begin() + static_cast<std::ptrdiff_t>(i) + 1;
                                 f != file_contents.end();
                                 ++f)
                            {
                                std::string_view spec = util::strip((*f));
                                if (!spec.empty() && spec[0] != '#')
                                {
                                    explicit_specs.push_back(*f);
                                }
                            }

                            specs.clear_values();
                            specs.set_value(explicit_specs);
                            config.at("explicit_install").set_value(true);

                            return;
                        }
                    }

                    // If we reach here, we have a file with no explicit env, and the content of the
                    // file just lists MatchSpecs.
                    if (specs.cli_configured())
                    {
                        auto current_specs = specs.cli_value<std::vector<std::string>>();
                        current_specs.insert(
                            current_specs.end(),
                            file_contents.cbegin(),
                            file_contents.cend()
                        );
                        specs.set_cli_value(current_specs);
                    }
                    else
                    {
                        if (!file_contents.empty())
                        {
                            specs.set_cli_value(file_contents);
                        }
                    }
                }
            }
        }

        void channels_hook(Configuration& config, std::vector<std::string>& channels)
        {
            auto& config_channels = config.at("channels");
            std::vector<std::string> cli_channels;

            if (config_channels.cli_configured())
            {
                cli_channels = config_channels.cli_value<std::vector<std::string>>();
                auto it = find(cli_channels.begin(), cli_channels.end(), "nodefaults");
                if (it != cli_channels.end())
                {
                    cli_channels.erase(it);
                    channels = cli_channels;
                }
            }
        }

        void
        get_all_pkg_info(PackageDiff::package_diff_map::value_type& pkg, solver::libsolv::Database& db)
        {
            const auto ms = pkg.second.name + "==" + pkg.second.version + " ="
                            + pkg.second.build_string;
            db.for_each_package_matching(
                specs::MatchSpec::parse(ms).value(),
                [&](specs::PackageInfo&& pkg_info) { pkg.second = pkg_info; }
            );
        }

        void
        install_revision(Context& ctx, ChannelContext& channel_context, std::size_t target_revision)
        {
            auto maybe_prefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
            if (!maybe_prefix_data)
            {
                throw std::runtime_error(maybe_prefix_data.error().what());
            }
            PrefixData& prefix_data = maybe_prefix_data.value();
            const auto user_requests = prefix_data.history().get_user_requests();

            PackageDiff pkg_diff = PackageDiff::from_revision(user_requests, target_revision);
            auto removed_pkg_diff = pkg_diff.removed_pkg_diff;
            auto installed_pkg_diff = pkg_diff.installed_pkg_diff;

            MultiPackageCache package_caches{ ctx.pkgs_dirs, ctx.validation_params };

            solver::libsolv::Database db{ channel_context.params() };
            add_logger_to_database(db);

            auto maybe_load = load_channels(ctx, channel_context, db, package_caches);
            if (!maybe_load)
            {
                throw std::runtime_error(maybe_load.error().what());
            }

            load_installed_packages_in_database(ctx, db, prefix_data);

            for (auto& pkg : removed_pkg_diff)
            {
                get_all_pkg_info(pkg, db);
            }
            for (auto& pkg : installed_pkg_diff)
            {
                get_all_pkg_info(pkg, db);
            }

            auto execute_transaction = [&](MTransaction& transaction)
            {
                if (ctx.output_params.json)
                {
                    transaction.log_json();
                }

                auto prompt_entry = transaction.prompt(ctx, channel_context);
                if (prompt_entry)
                {
                    transaction.execute(ctx, channel_context, prefix_data);
                }
                return prompt_entry;
            };

            std::vector<specs::PackageInfo> pkgs_to_remove;
            std::vector<specs::PackageInfo> pkgs_to_install;
            for (const auto& [_, pkg] : installed_pkg_diff)
            {
                pkgs_to_remove.push_back(pkg);
            }
            for (const auto& [_, pkg] : removed_pkg_diff)
            {
                pkgs_to_install.push_back(pkg);
            }
            auto transaction = MTransaction(ctx, db, pkgs_to_remove, pkgs_to_install, package_caches);
            execute_transaction(transaction);
        }
    }  // detail
}  // mamba
