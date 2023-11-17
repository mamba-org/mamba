// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <stdexcept>

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <reproc++/run.hpp>
#include <reproc/reproc.h>

#include "mamba/api/channel_loader.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"
#include "mamba/core/activation.hpp"
#include "mamba/core/channel.hpp"
#include "mamba/core/download.hpp"
#include "mamba/core/env_lockfile.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/environments_manager.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/pinning.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    namespace
    {
        using command_args = std::vector<std::string>;

        tl::expected<command_args, std::runtime_error> get_other_pkg_mgr_install_instructions(
            const std::string& name,
            const std::string& target_prefix,
            const fs::u8path& spec_file
        )
        {
            const auto get_python_path = [&]
            { return env::which("python", get_path_dirs(target_prefix)).string(); };

            const std::unordered_map<std::string, command_args> other_pkg_mgr_install_instructions{
                { "pip",
                  { get_python_path(), "-m", "pip", "install", "-r", spec_file, "--no-input" } },
                { "pip --no-deps",
                  { get_python_path(), "-m", "pip", "install", "--no-deps", "-r", spec_file, "--no-input" } }
            };

            auto found_it = other_pkg_mgr_install_instructions.find(name);
            if (found_it != other_pkg_mgr_install_instructions.end())
            {
                return found_it->second;
            }
            else
            {
                return tl::unexpected(std::runtime_error(
                    fmt::format("no install instruction found for package manager '{}'", name)
                ));
            }
        }

    }

    bool reproc_killed(int status)
    {
        return status == REPROC_SIGKILL;
    }

    bool reproc_terminated(int status)
    {
        return status == REPROC_SIGTERM;
    }

    void assert_reproc_success(const reproc::options& options, int status, std::error_code ec)
    {
        bool killed_not_an_err = (options.stop.first.action == reproc::stop::kill)
                                 || (options.stop.second.action == reproc::stop::kill)
                                 || (options.stop.third.action == reproc::stop::kill);

        bool terminated_not_an_err = (options.stop.first.action == reproc::stop::terminate)
                                     || (options.stop.second.action == reproc::stop::terminate)
                                     || (options.stop.third.action == reproc::stop::terminate);

        if (ec || (!killed_not_an_err && reproc_killed(status))
            || (!terminated_not_an_err && reproc_terminated(status)))
        {
            if (ec)
            {
                LOG_ERROR << "Subprocess call failed: " << ec.message();
            }
            else if (reproc_killed(status))
            {
                LOG_ERROR << "Subprocess call failed (killed)";
            }
            else
            {
                LOG_ERROR << "Subprocess call failed (terminated)";
            }
            throw std::runtime_error("Subprocess call failed. Aborting.");
        }
    }

    auto install_for_other_pkgmgr(const Context& ctx, const detail::other_pkg_mgr_spec& other_spec)
    {
        const auto& pkg_mgr = other_spec.pkg_mgr;
        const auto& deps = other_spec.deps;
        const auto& cwd = other_spec.cwd;

        TemporaryFile specs("mambaf", "", cwd);
        {
            std::ofstream specs_f = open_ofstream(specs.path());
            for (auto& d : deps)
            {
                specs_f << d.c_str() << '\n';
            }
        }

        command_args install_instructions = [&]
        {
            const auto maybe_instructions = get_other_pkg_mgr_install_instructions(
                pkg_mgr,
                ctx.prefix_params.target_prefix.string(),
                specs.path()
            );
            if (maybe_instructions)
            {
                return maybe_instructions.value();
            }
            else
            {
                throw maybe_instructions.error();
            }
        }();

        auto [wrapped_command, tmpfile] = prepare_wrapped_call(
            ctx,
            ctx.prefix_params.target_prefix,
            install_instructions
        );

        reproc::options options;
        options.redirect.parent = true;
        options.working_directory = cwd.c_str();

        Console::stream() << fmt::format(
            ctx.graphics_params.palette.external,
            "\nInstalling {} packages: {}",
            pkg_mgr,
            fmt::join(deps, ", ")
        );
        fmt::print(LOG_INFO, "Calling: {}", fmt::join(install_instructions, " "));

        auto [status, ec] = reproc::run(wrapped_command, options);
        assert_reproc_success(options, status, ec);
        if (status != 0)
        {
            throw std::runtime_error("pip failed to install packages");
        }
    }

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
                throw std::runtime_error("Couldn't parse selector. Value not in [unix, linux, "
                                         "osx, win] or additional whitespaces found.");
            }

            return found_it->second;
        }

        yaml_file_contents read_yaml_file(fs::u8path yaml_file, const std::string platform)
        {
            auto file = fs::weakly_canonical(util::expand_home(yaml_file.string()));
            if (!fs::exists(file))
            {
                LOG_ERROR << "YAML spec file '" << file.string() << "' not found";
                throw std::runtime_error("File not found. Aborting.");
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
                LOG_ERROR << "No 'dependencies' specified in YAML spec file '" << file.string()
                          << "'";
                throw std::runtime_error("Invalid spec file. Aborting.");
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
                            const auto yaml_parent_path = fs::absolute(yaml_file).parent_path().string(
                            );
                            result.others_pkg_mgrs_specs.push_back({
                                "pip",
                                map_el.second.as<std::vector<std::string>>(),
                                yaml_parent_path,
                            });
                            has_pip_deps = true;
                        }
                    }
                }
            }

            std::vector<std::string> dependencies;
            try
            {
                dependencies = final_deps.as<std::vector<std::string>>();
            }
            catch (const YAML::Exception& e)
            {
                LOG_ERROR << "Bad conversion of 'dependencies' to a vector of string: " << final_deps;
                throw e;
            }

            if (has_pip_deps && !std::count(dependencies.begin(), dependencies.end(), "pip"))
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
            {
                LOG_DEBUG << "No env 'name' specified in YAML spec file '" << file.string() << "'";
            }
            return result;
        }

        std::tuple<std::vector<PackageInfo>, std::vector<MatchSpec>>
        parse_urls_to_package_info(const std::vector<std::string>& urls, ChannelContext& channel_context)
        {
            std::vector<PackageInfo> pi_result;
            std::vector<MatchSpec> ms_result;
            for (auto& u : urls)
            {
                if (util::strip(u).size() == 0)
                {
                    continue;
                }
                std::size_t hash = u.find_first_of('#');
                MatchSpec ms(u.substr(0, hash), channel_context);
                PackageInfo p(ms.name);
                p.url = ms.url;
                p.build_string = ms.build_string;
                p.version = ms.version;
                p.channel = ms.channel;
                p.fn = ms.fn;

                if (hash != std::string::npos)
                {
                    p.md5 = u.substr(hash + 1);
                    ms.brackets["md5"] = u.substr(hash + 1);
                }
                pi_result.push_back(p);
                ms_result.push_back(ms);
            }
            return std::make_tuple(pi_result, ms_result);
        }

        bool operator==(const other_pkg_mgr_spec& s1, const other_pkg_mgr_spec& s2)
        {
            return (s1.pkg_mgr == s2.pkg_mgr) && (s1.deps == s2.deps) && (s1.cwd == s2.cwd);
        }
    }

    void install(Configuration& config)
    {
        config.at("create_base").set_value(true);
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(
                MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_NOT_ALLOW_MISSING_PREFIX
                | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_EXPECT_EXISTING_PREFIX
            );
        config.load();

        auto& install_specs = config.at("specs").value<std::vector<std::string>>();
        auto& use_explicit = config.at("explicit_install").value<bool>();

        auto& context = config.context();
        ChannelContext channel_context{ context };

        if (context.env_lockfile)
        {
            const auto lockfile_path = context.env_lockfile.value();
            LOG_DEBUG << "Lockfile: " << lockfile_path;
            install_lockfile_specs(
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
                install_explicit_specs(channel_context, install_specs, false);
            }
            else
            {
                mamba::install_specs(channel_context, config, install_specs, false);
            }
        }
        else
        {
            Console::instance().print("Nothing to do.");
        }
    }

    int RETRY_SUBDIR_FETCH = 1 << 0;
    int RETRY_SOLVE_ERROR = 1 << 1;

    void install_specs(
        ChannelContext& channel_context,
        const Configuration& config,
        const std::vector<std::string>& specs,
        bool create_env,
        bool remove_prefix_on_failure,
        int solver_flag,
        int is_retry
    )
    {
        assert(&config.context() == &channel_context.context());
        Context& ctx = channel_context.context();

        auto& no_pin = config.at("no_pin").value<bool>();
        auto& no_py_pin = config.at("no_py_pin").value<bool>();
        auto& freeze_installed = config.at("freeze_installed").value<bool>();
        auto& force_reinstall = config.at("force_reinstall").value<bool>();
        auto& no_deps = config.at("no_deps").value<bool>();
        auto& only_deps = config.at("only_deps").value<bool>();
        auto& retry_clean_cache = config.at("retry_clean_cache").value<bool>();

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

        // add channels from specs
        for (const auto& s : specs)
        {
            if (auto m = MatchSpec{ s, channel_context }; !m.channel.empty())
            {
                ctx.channels.push_back(m.channel);
            }
        }

        if (ctx.channels.empty() && !ctx.offline)
        {
            LOG_WARNING << "No 'channels' specified";
        }

        MPool pool{ channel_context };
        // functions implied in 'and_then' coding-styles must return the same type
        // which limits this syntax
        /*auto exp_prefix_data = load_channels(pool, package_caches, is_retry)
                               .and_then([&ctx](const auto&) { return
           PrefixData::create(ctx.prefix_params.target_prefix); } ) .map_error([](const mamba_error&
           err) { throw std::runtime_error(err.what());
                                });*/
        auto exp_load = load_channels(pool, package_caches, is_retry);
        if (!exp_load)
        {
            throw std::runtime_error(exp_load.error().what());
        }

        auto exp_prefix_data = PrefixData::create(
            ctx.prefix_params.target_prefix,
            pool.channel_context()
        );
        if (!exp_prefix_data)
        {
            throw std::runtime_error(exp_prefix_data.error().what());
        }
        PrefixData& prefix_data = exp_prefix_data.value();

        std::vector<std::string> prefix_pkgs;
        for (auto& it : prefix_data.records())
        {
            prefix_pkgs.push_back(it.first);
        }

        prefix_data.add_packages(get_virtual_packages(ctx));

        MRepo(pool, prefix_data);

        MSolver solver(
            pool,
            {
                { SOLVER_FLAG_ALLOW_UNINSTALL, ctx.allow_uninstall },
                { SOLVER_FLAG_ALLOW_DOWNGRADE, ctx.allow_downgrade },
                { SOLVER_FLAG_STRICT_REPO_PRIORITY, ctx.channel_priority == ChannelPriority::Strict },
            }
        );

        solver.set_flags({
            /* .keep_dependencies= */ !no_deps,
            /* .keep_specs= */ !only_deps,
            /* .force_reinstall= */ force_reinstall,
        });

        if (freeze_installed && !prefix_pkgs.empty())
        {
            LOG_INFO << "Locking environment: " << prefix_pkgs.size() << " packages freezed";
            solver.add_jobs(prefix_pkgs, SOLVER_LOCK);
        }

        if (!no_pin)
        {
            solver.add_pins(file_pins(prefix_data.path() / "conda-meta" / "pinned"));
            solver.add_pins(ctx.pinned_packages);
        }

        if (!no_py_pin)
        {
            auto py_pin = python_pin(prefix_data, specs);
            if (!py_pin.empty())
            {
                solver.add_pin(py_pin);
            }
        }
        if (!solver.pinned_specs().empty())
        {
            std::vector<std::string> pinned_str;
            for (auto& ms : solver.pinned_specs())
            {
                pinned_str.push_back("  - " + ms.conda_build_form() + "\n");
            }
            Console::instance().print("\nPinned packages:\n" + util::join("", pinned_str));
        }

        // FRAGILE this must be called after pins be before jobs in current ``MPool``
        pool.create_whatprovides();

        solver.add_jobs(specs, solver_flag);

        bool success = solver.try_solve();
        if (!success)
        {
            solver.explain_problems(LOG_ERROR);
            if (retry_clean_cache && !(is_retry & RETRY_SOLVE_ERROR))
            {
                ctx.local_repodata_ttl = 2;
                return install_specs(
                    channel_context,
                    config,
                    specs,
                    create_env,
                    remove_prefix_on_failure,
                    solver_flag,
                    is_retry | RETRY_SOLVE_ERROR
                );
            }
            if (freeze_installed)
            {
                Console::instance().print("Possible hints:\n  - 'freeze_installed' is turned on\n");
            }

            if (ctx.output_params.json)
            {
                Console::instance().json_write({ { "success", false },
                                                 { "solver_problems", solver.all_problems() } });
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

        MTransaction trans(pool, solver, package_caches);

        if (ctx.output_params.json)
        {
            trans.log_json();
        }

        Console::stream();

        if (trans.prompt())
        {
            if (create_env && !ctx.dry_run)
            {
                detail::create_target_directory(ctx, ctx.prefix_params.target_prefix);
            }

            trans.execute(prefix_data);

            for (auto other_spec :
                 config.at("others_pkg_mgrs_specs").value<std::vector<detail::other_pkg_mgr_spec>>())
            {
                install_for_other_pkgmgr(ctx, other_spec);
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

    namespace detail
    {
        // TransactionFunc: (MPool& pool, MultiPackageCache& package_caches) -> MTransaction
        template <typename TransactionFunc>
        void install_explicit_with_transaction(
            ChannelContext& channel_context,
            TransactionFunc create_transaction,
            bool create_env,
            bool remove_prefix_on_failure
        )
        {
            MPool pool{ channel_context };
            auto& ctx = channel_context.context();
            auto exp_prefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
            if (!exp_prefix_data)
            {
                // TODO: propagate tl::expected mechanism
                throw std::runtime_error("could not load prefix data");
            }
            PrefixData& prefix_data = exp_prefix_data.value();

            MultiPackageCache pkg_caches(ctx.pkgs_dirs, ctx.validation_params);
            prefix_data.add_packages(get_virtual_packages(ctx));
            MRepo(pool, prefix_data);  // Potentially re-alloc (moves in memory) Solvables
                                       // in the pool

            std::vector<detail::other_pkg_mgr_spec> others;
            // Note that the Transaction will gather the Solvables,
            // so they must have been ready in the pool before this line
            auto transaction = create_transaction(pool, pkg_caches, others);

            std::vector<LockFile> lock_pkgs;

            for (auto& c : ctx.pkgs_dirs)
            {
                lock_pkgs.push_back(LockFile(c));
            }

            if (ctx.output_params.json)
            {
                transaction.log_json();
            }

            if (transaction.prompt())
            {
                if (create_env && !ctx.dry_run)
                {
                    detail::create_target_directory(ctx, ctx.prefix_params.target_prefix);
                }

                transaction.execute(prefix_data);

                for (auto other_spec : others)
                {
                    install_for_other_pkgmgr(ctx, other_spec);
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
        ChannelContext& channel_context,
        const std::vector<std::string>& specs,
        bool create_env,
        bool remove_prefix_on_failure
    )
    {
        detail::install_explicit_with_transaction(
            channel_context,
            [&](auto& pool, auto& pkg_caches, auto& others)
            { return create_explicit_transaction_from_urls(pool, specs, pkg_caches, others); },
            create_env,
            remove_prefix_on_failure
        );
    }

    void install_lockfile_specs(
        ChannelContext& channel_context,
        const std::string& lockfile,

        const std::vector<std::string>& categories,
        bool create_env,
        bool remove_prefix_on_failure
    )
    {
        std::unique_ptr<TemporaryFile> tmp_lock_file;
        fs::u8path file;

        if (lockfile.find("://") != std::string::npos)
        {
            LOG_INFO << "Downloading lockfile";
            tmp_lock_file = std::make_unique<TemporaryFile>();
            DownloadRequest request("Environment Lockfile", lockfile, tmp_lock_file->path());
            DownloadResult res = download(std::move(request), channel_context.context());

            if (!res || res.value().transfer.http_status != 200)
            {
                throw std::runtime_error(
                    fmt::format("Could not download environment lockfile from {}", lockfile)
                );
            }

            file = tmp_lock_file->path();
        }
        else
        {
            file = lockfile;
        }

        detail::install_explicit_with_transaction(
            channel_context,
            [&](auto& pool, auto& pkg_caches, auto& others) {
                return create_explicit_transaction_from_lockfile(pool, file, categories, pkg_caches, others);
            },
            create_env,
            remove_prefix_on_failure
        );
    }

    namespace detail
    {
        void create_empty_target(const Context& context, const fs::u8path& prefix)
        {
            detail::create_target_directory(context, prefix);

            Console::instance().print(util::join(
                "",
                std::vector<std::string>({ "Empty environment created at prefix: ", prefix.string() })
            ));
            Console::instance().json_write({ { "success", true } });
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

            auto& context = config.context();

            if (file_specs.size() == 0)
            {
                return;
            }

            for (const auto& file : file_specs)
            {
                if (is_yaml_file_name(file) && file_specs.size() != 1)
                {
                    throw std::runtime_error("Can only handle 1 yaml file!");
                }
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
                    const auto parse_result = read_yaml_file(file, context.platform);

                    if (parse_result.channels.size() != 0)
                    {
                        std::vector<std::string> updated_channels;
                        if (channels.cli_configured())
                        {
                            updated_channels = channels.cli_value<std::vector<std::string>>();
                        }
                        for (auto& c : parse_result.channels)
                        {
                            updated_channels.push_back(c);
                        }
                        channels.set_cli_value(updated_channels);
                    }

                    if (parse_result.name.size() != 0)
                    {
                        env_name.set_cli_yaml_value(parse_result.name);
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

                    others_pkg_mgrs_specs.set_value(parse_result.others_pkg_mgrs_specs);
                }
                else
                {
                    const std::vector<std::string> file_contents = read_lines(file);
                    if (file_contents.size() == 0)
                    {
                        throw std::runtime_error(util::concat("Got an empty file: ", file));
                    }
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

                    std::vector<std::string> f_specs;
                    for (auto& line : file_contents)
                    {
                        if (line[0] != '#' && line[0] != '@')
                        {
                            f_specs.push_back(line);
                        }
                    }

                    if (specs.cli_configured())
                    {
                        auto current_specs = specs.cli_value<std::vector<std::string>>();
                        current_specs.insert(current_specs.end(), f_specs.cbegin(), f_specs.cend());
                        specs.set_cli_value(current_specs);
                    }
                    else
                    {
                        if (!f_specs.empty())
                        {
                            specs.set_cli_value(f_specs);
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
    }  // detail
}  // mamba
