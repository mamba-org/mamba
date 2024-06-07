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
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/env_lockfile.hpp"
#include "mamba/core/environments_manager.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/pinning.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/download/downloader.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/solver/libsolv/solver.hpp"
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
            { return util::which_in("python", get_path_dirs(target_prefix)).string(); };

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

        bool operator==(const other_pkg_mgr_spec& s1, const other_pkg_mgr_spec& s2)
        {
            return (s1.pkg_mgr == s2.pkg_mgr) && (s1.deps == s2.deps) && (s1.cwd == s2.cwd);
        }
    }

    void install(Configuration& config)
    {
        auto& ctx = config.context();

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

    auto
    create_install_request(PrefixData& prefix_data, std::vector<std::string> specs, bool freeze_installed)
        -> solver::Request
    {
        using Request = solver::Request;

        const auto& prefix_pkgs = prefix_data.records();

        auto request = Request();
        request.jobs.reserve(specs.size() + freeze_installed * prefix_pkgs.size());

        // Consider if a FreezeAll type in Request is relevant?
        if (freeze_installed && !prefix_pkgs.empty())
        {
            LOG_INFO << "Locking environment: " << prefix_pkgs.size() << " packages freezed";
            for (const auto& [name, pkg] : prefix_pkgs)
            {
                request.jobs.emplace_back(Request::Freeze{
                    specs::MatchSpec::parse(name)
                        .or_else([](specs::ParseError&& err) { throw std::move(err); })
                        .value(),
                });
            }
        }

        for (const auto& s : specs)
        {
            request.jobs.emplace_back(Request::Install{
                specs::MatchSpec::parse(s)
                    .or_else([](specs::ParseError&& err) { throw std::move(err); })
                    .value(),
            });
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

        LOG_WARNING << "DEBUG WARNING: no_py_pin = " << (no_py_pin ? "true" : "false");

        const auto estimated_jobs_count = request.jobs.size()
                                          + (!no_pin) * ctx.pinned_packages.size() + !no_py_pin;
        request.jobs.reserve(estimated_jobs_count);
        if (!no_pin)
        {
            for (const auto& pin : file_pins(prefix_data.path() / "conda-meta" / "pinned"))
            {
                request.jobs.emplace_back(Request::Pin{
                    specs::MatchSpec::parse(pin)
                        .or_else([](specs::ParseError&& err) { throw std::move(err); })
                        .value(),
                });
            }
            for (const auto& pin : ctx.pinned_packages)
            {
                request.jobs.emplace_back(Request::Pin{
                    specs::MatchSpec::parse(pin)
                        .or_else([](specs::ParseError&& err) { throw std::move(err); })
                        .value(),
                });
            }
        }

        if (!no_py_pin)
        {
            auto py_pin = python_pin(prefix_data, specs);
            if (!py_pin.empty())
            {
                request.jobs.emplace_back(Request::Pin{
                    specs::MatchSpec::parse(py_pin)
                        .or_else([](specs::ParseError&& err) { throw std::move(err); })
                        .value(),
                });
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
                        out << "  - " << item.spec.str() << '\n';
                    }
                },
                req
            );
        }
    }

    namespace
    {
        void install_specs_impl(
            Context& ctx,
            ChannelContext& channel_context,
            const Configuration& config,
            const std::vector<std::string>& specs,
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

            if (ctx.prefix_params.target_prefix.empty())
            {
                throw std::runtime_error("No active target prefix");
            }
            if (!fs::exists(ctx.prefix_params.target_prefix) && create_env == false)
            {
                throw std::runtime_error(fmt::format(
                    "Prefix does not exist at: {}",
                    ctx.prefix_params.target_prefix.string()
                ));
            }

            MultiPackageCache package_caches{ ctx.pkgs_dirs, ctx.validation_params };

            // add channels from specs
            for (const auto& s : specs)
            {
                if (auto ms = specs::MatchSpec::parse(s); ms && ms->channel().has_value())
                {
                    ctx.channels.push_back(ms->channel()->str());
                }
            }

            if (ctx.channels.empty() && !ctx.offline)
            {
                LOG_WARNING << "No 'channels' specified";
            }

            solver::libsolv::Database db{ channel_context.params() };
            add_spdlog_logger_to_database(db);
            // functions implied in 'and_then' coding-styles must return the same type
            // which limits this syntax
            /*auto exp_prefix_data = load_channels(pool, package_caches)
                                   .and_then([&ctx](const auto&) { return
               PrefixData::create(ctx.prefix_params.target_prefix); } ) .map_error([](const
               mamba_error& err) { throw std::runtime_error(err.what());
                                    });*/
            auto exp_load = load_channels(ctx, channel_context, db, package_caches);
            if (!exp_load)
            {
                throw std::runtime_error(exp_load.error().what());
            }

            auto exp_prefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
            if (!exp_prefix_data)
            {
                throw std::runtime_error(exp_prefix_data.error().what());
            }
            PrefixData& prefix_data = exp_prefix_data.value();

            load_installed_packages_in_database(ctx, db, prefix_data);


            auto request = create_install_request(prefix_data, specs, freeze_installed);
            add_pins_to_request(request, ctx, prefix_data, specs, no_pin, no_py_pin);
            request.flags = ctx.solver_flags;

            {
                auto out = Console::stream();
                print_request_pins_to(request, out);
                // Console stream prints on destruction
            }

            auto outcome = solver::libsolv::Solver().solve(db, request).value();

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
                    return install_specs_impl(
                        ctx,
                        channel_context,
                        config,
                        specs,
                        create_env,
                        remove_prefix_on_failure,
                        true
                    );
                }
                if (freeze_installed)
                {
                    Console::instance().print("Possible hints:\n  - 'freeze_installed' is turned on\n"
                    );
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
            auto trans = [&](auto db)
            {
                return MTransaction(  //
                    ctx,
                    db,
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

                trans.execute(ctx, channel_context, prefix_data);

                for (auto other_spec : config.at("others_pkg_mgrs_specs")
                                           .value<std::vector<detail::other_pkg_mgr_spec>>())
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

    void install_specs(
        Context& ctx,
        ChannelContext& channel_context,
        const Configuration& config,
        const std::vector<std::string>& specs,
        bool create_env,
        bool remove_prefix_on_failure
    )
    {
        return install_specs_impl(
            ctx,
            channel_context,
            config,
            specs,
            create_env,
            remove_prefix_on_failure,
            false
        );
    }

    namespace
    {

        // TransactionFunc: (Database& pool, MultiPackageCache& package_caches) -> MTransaction
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
            solver::libsolv::Database db{ channel_context.params() };
            add_spdlog_logger_to_database(db);

            init_channels(ctx, channel_context);
            // Some use cases provide a list of explicit specs, but an empty
            // context. We need to create channels from the specs to be able
            // to download packages.
            init_channels_from_package_urls(ctx, channel_context, specs);
            auto exp_prefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
            if (!exp_prefix_data)
            {
                // TODO: propagate tl::expected mechanism
                throw std::runtime_error("could not load prefix data");
            }
            PrefixData& prefix_data = exp_prefix_data.value();

            MultiPackageCache pkg_caches(ctx.pkgs_dirs, ctx.validation_params);

            load_installed_packages_in_database(ctx, db, prefix_data);

            std::vector<detail::other_pkg_mgr_spec> others;
            // Note that the Transaction will gather the Solvables,
            // so they must have been ready in the pool before this line
            auto transaction = create_transaction(db, pkg_caches, others);

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
        std::unique_ptr<TemporaryFile> tmp_lock_file;
        fs::u8path file;

        if (lockfile.find("://") != std::string::npos)
        {
            LOG_INFO << "Downloading lockfile";
            tmp_lock_file = std::make_unique<TemporaryFile>();
            download::Request request(
                "Environment Lockfile",
                download::MirrorName(""),
                lockfile,
                tmp_lock_file->path()
            );
            const download::Result res = download::download(std::move(request), ctx.mirrors, ctx);

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
