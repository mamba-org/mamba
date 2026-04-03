// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cctype>
#include <fstream>
#include <unordered_set>

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>
#include <reproc++/run.hpp>
#include <reproc/reproc.h>

// TODO includes to be removed after moving some functions/structs around
#include "mamba/api/channel_loader.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"  // other_pkg_mgr_spec
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/environments_manager.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/shard_python_minor_prefilter.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/request.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/version_spec.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/string.hpp"

#include "utils.hpp"

namespace mamba
{
    namespace
    {
        tl::expected<command_args, std::runtime_error> get_pkg_mgr_install_command(
            const std::string& name,
            const std::string& target_prefix,
            const fs::u8path& spec_file,
            pip::Update update,
            const Context::OutputParams& output_params
        )
        {
            const auto get_python_path = [&]
            { return util::which_in("python", util::get_path_dirs(target_prefix)).string(); };

            const auto get_uv_path = [&]
            { return util::which_in("uv", util::get_path_dirs(target_prefix)).string(); };

            command_args cmd = [&]
            {
                if (name == "uv")
                {
                    return command_args{ get_uv_path() };
                }
                else
                {
                    return command_args{ get_python_path(), "-m" };
                }
            }();

            cmd.insert(cmd.end(), { "pip", "install" });
            command_args cmd_extension = { "-r", spec_file };

            // Only use --quiet when --json or --quiet is used to avoid interfering with output
            if (output_params.json || output_params.quiet)
            {
                cmd_extension.push_back("--quiet");
            }

            if (name != "uv")
            {
                cmd_extension.push_back("--no-input");
            }

            if (update == pip::Update::Yes)
            {
                cmd.push_back("-U");
            }

            if (name == "pip --no-deps")
            {
                cmd.push_back("--no-deps");
            }

            cmd.insert(cmd.end(), cmd_extension.begin(), cmd_extension.end());
            const std::unordered_map<std::string, command_args> pip_install_command{
                { "pip", cmd },
                { "pip --no-deps", cmd },
                { "uv", cmd },
            };

            auto found_it = pip_install_command.find(name);
            if (found_it != pip_install_command.end())
            {
                return found_it->second;
            }
            else
            {
                return tl::unexpected(
                    std::runtime_error(
                        fmt::format(
                            "no {} instruction found for package manager '{}'",
                            (update == pip::Update::Yes) ? "update" : "install",
                            name
                        )
                    )
                );
            }
        }

        std::optional<specs::Version>
        installed_python_minor_for_prefix(const fs::u8path& target_prefix)
        {
            const auto parse_minor = [](std::string_view v) -> std::optional<specs::Version>
            {
                auto maybe_version = specs::Version::parse(std::string(v));
                if (maybe_version.has_value())
                {
                    return maybe_version.value();
                }
                return std::nullopt;
            };
            const auto conda_meta = target_prefix / "conda-meta";
            if (!fs::exists(conda_meta) || !fs::is_directory(conda_meta))
            {
                return std::nullopt;
            }

            for (const auto& entry : fs::directory_iterator(conda_meta))
            {
                if (!entry.is_regular_file() || entry.path().extension() != ".json")
                {
                    continue;
                }
                std::ifstream infile(entry.path().std_path());
                if (!infile.is_open())
                {
                    continue;
                }
                nlohmann::json j;
                try
                {
                    infile >> j;
                }
                catch (const std::exception&)
                {
                    continue;
                }
                if (!j.is_object() || j.value("name", "") != "python")
                {
                    continue;
                }
                const std::string version = j.value("version", "");
                auto dot = version.find('.');
                if (dot == std::string::npos)
                {
                    continue;
                }
                auto second_dot = version.find('.', dot + 1);
                if (second_dot == std::string::npos)
                {
                    return parse_minor(version);
                }
                return parse_minor(version.substr(0, second_dot));
            }
            return std::nullopt;
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

    tl::expected<command_args, std::runtime_error> install_for_other_pkgmgr(
        const Context& ctx,
        const detail::other_pkg_mgr_spec& other_spec,
        pip::Update update
    )
    {
        const auto& pkg_mgr = other_spec.pkg_mgr;
        const auto& deps = other_spec.deps;
        const auto& cwd = other_spec.cwd;

        LOG_WARNING << fmt::format(
            "You are using '{}' as an additional package manager.\nBe aware that packages installed with '{}' are managed independently from 'conda-forge' channel.",
            pkg_mgr,
            pkg_mgr
        );

        TemporaryFile specs("mambaf", "", cwd);
        {
            std::ofstream specs_f = open_ofstream(specs.path());
            for (auto& d : deps)
            {
                specs_f << d.c_str() << '\n';
            }
        }

        command_args command = [&]
        {
            const auto maybe_command = get_pkg_mgr_install_command(
                pkg_mgr,
                ctx.prefix_params.target_prefix.string(),
                specs.path(),
                update,
                ctx.output_params
            );
            if (maybe_command)
            {
                return maybe_command.value();
            }
            else
            {
                throw maybe_command.error();
            }
        }();

        auto [wrapped_command, tmpfile] = prepare_wrapped_call(
            ctx.prefix_params,
            command,
            ctx.command_params.is_mamba_exe
        );

        reproc::options options;
        options.redirect.parent = true;
        options.working_directory = cwd.c_str();

        Console::stream() << fmt::format(
            ctx.graphics_params.palette.external,
            "\n{} {} packages: {}",
            (update == pip::Update::Yes) ? "Updating" : "Installing",
            pkg_mgr,
            fmt::join(deps, ", ")
        );

        fmt::print(LOG_INFO, "Calling: {}", fmt::join(command, " "));

        auto [status, ec] = reproc::run(wrapped_command, options);
        assert_reproc_success(options, status, ec);
        if (status != 0)
        {
            throw std::runtime_error(
                fmt::format("pip failed to {} packages", (update == pip::Update::Yes) ? "update" : "install")
            );
        }

        return command;
    }

    void
    populate_context_channels_from_specs(const std::vector<std::string>& raw_matchspecs, Context& context)
    {
        for (const auto& s : raw_matchspecs)
        {
            if (auto ms = specs::MatchSpec::parse(s); ms && ms->channel().has_value())
            {
                auto channel_name = ms->channel()->str();
                auto channel_is_absent = std::find(
                                             context.channels.begin(),
                                             context.channels.end(),
                                             channel_name
                                         )
                                         == context.channels.end();
                if (channel_is_absent)
                {
                    context.channels.push_back(channel_name);
                }
            }
        }
    }

    std::vector<std::string> extract_package_names_from_specs(const std::vector<std::string>& specs)
    {
        std::vector<std::string> names;
        for (const auto& s : specs)
        {
            auto parsed = specs::MatchSpec::parse(s);
            if (parsed && !parsed->name().is_free())
            {
                auto name = parsed->name().to_string();
                if (!name.empty())
                {
                    names.push_back(std::move(name));
                }
            }
        }
        return names;
    }

    void add_pip_if_python(std::vector<std::string>& root_packages)
    {
        bool has_python = false;
        bool has_pip = false;
        for (const auto& pkg : root_packages)
        {
            if (pkg == "python")
            {
                has_python = true;
            }
            if (pkg == "pip")
            {
                has_pip = true;
            }
        }
        if (has_python && !has_pip)
        {
            root_packages.push_back("pip");
        }
    }

    std::vector<std::string> build_sharded_root_packages(
        const Context& ctx,
        ChannelContext& channel_context,
        const std::vector<std::string>& raw_specs
    )
    {
        std::vector<std::string> root_packages = extract_package_names_from_specs(raw_specs);
        add_pip_if_python(root_packages);
        if (!fs::exists(ctx.prefix_params.target_prefix))
        {
            return root_packages;
        }

        auto maybe_prefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
        if (!maybe_prefix_data.has_value())
        {
            LOG_DEBUG << "Failed to load prefix data for sharded root expansion: "
                      << maybe_prefix_data.error().what();
            return root_packages;
        }

        auto prefix_data = std::move(maybe_prefix_data).value();
        std::unordered_set<std::string> seen(root_packages.begin(), root_packages.end());
        for (const auto& [name, /*record*/ _] : prefix_data.records())
        {
            if (seen.insert(name).second)
            {
                root_packages.push_back(name);
            }
        }
        return root_packages;
    }

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

    solver::libsolv::Solver::Outcome solve_request_with_status(
        bool experimental_matchspec_parsing,
        solver::libsolv::Database& db,
        const solver::Request& request
    )
    {
        if (Console::can_report_status())
        {
            Console::instance().print(
                fmt::format("{:<85} {:>20}", "Resolving Environment", "⧖ Starting")
            );
        }
        auto outcome = solver::libsolv::Solver()
                           .solve(
                               db,
                               request,
                               experimental_matchspec_parsing
                                   ? solver::libsolv::MatchSpecParser::Mamba
                                   : solver::libsolv::MatchSpecParser::Mixed
                           )
                           .value();
        if (Console::can_report_status())
        {
            Console::instance().print(fmt::format("{:<85} {:>20}", "Resolving Environment", "✔ Done"));
        }
        return outcome;
    }

    solver::libsolv::Database
    make_solver_database(bool experimental_matchspec_parsing, ChannelContext& channel_context)
    {
        solver::libsolv::Database db{
            channel_context.params(),
            {
                experimental_matchspec_parsing ? solver::libsolv::MatchSpecParser::Mamba
                                               : solver::libsolv::MatchSpecParser::Libsolv,
            },
        };
        add_logger_to_database(db);
        return db;
    }

    void configure_common_prefix_fallbacks(Configuration& config, bool create_base)
    {
        if (create_base)
        {
            config.at("create_base").set_value(true);
        }
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("use_default_prefix_fallback").set_value(true);
        config.at("use_root_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(
                MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_NOT_ALLOW_MISSING_PREFIX
                | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_EXPECT_EXISTING_PREFIX
            );
    }

    void validate_target_prefix_and_channels(const Context& ctx, bool create_env)
    {
        if (ctx.prefix_params.target_prefix.empty())
        {
            throw std::runtime_error("No active target prefix");
        }
        if (!fs::exists(ctx.prefix_params.target_prefix) && !create_env)
        {
            throw std::runtime_error(
                fmt::format("Prefix does not exist at: {}", ctx.prefix_params.target_prefix.string())
            );
        }
        if (ctx.channels.empty() && !ctx.offline)
        {
            LOG_WARNING << "No 'channels' specified";
        }
    }

    std::pair<solver::libsolv::Database, MultiPackageCache> prepare_solver_context(
        Context& ctx,
        ChannelContext& channel_context,
        const std::vector<std::string>& raw_specs,
        bool is_retry,
        bool no_py_pin
    )
    {
        populate_context_channels_from_specs(raw_specs, ctx);
        auto db = make_solver_database(ctx.experimental_matchspec_parsing, channel_context);

        MultiPackageCache package_caches(ctx.pkgs_dirs, ctx.validation_params);
        auto root_packages = ctx.repodata_use_shards
                                 ? build_sharded_root_packages(ctx, channel_context, raw_specs)
                                 : std::vector<std::string>{};

        const std::optional<specs::Version> python_minor_version_for_prefilter =
            [&]() -> std::optional<specs::Version>
        {
            if (no_py_pin)
            {
                LOG_DEBUG << "Shard python minor prefilter disabled (--no-py-pin).";
                return std::nullopt;
            }

            const auto maybe_explicit_requested_python_minor = extract_requested_python_minor(
                raw_specs
            );

            if (maybe_explicit_requested_python_minor.has_value())
            {
                LOG_DEBUG << "Pre-filtering shards using explicitly requested python minor version: "
                          << maybe_explicit_requested_python_minor.value().to_string();
                return maybe_explicit_requested_python_minor.value();
            }

            if (is_retry)
            {
                LOG_DEBUG << "Retry without prefiltering on any python minor version.";
                return std::nullopt;
            }

            const auto maybe_installed_python_minor = installed_python_minor_for_prefix(
                ctx.prefix_params.target_prefix
            );

            if (maybe_installed_python_minor.has_value())
            {
                LOG_DEBUG << "Pre-filtering shards using installed python minor version: "
                          << maybe_installed_python_minor.value().to_string();
                return maybe_installed_python_minor.value();
            }

            LOG_DEBUG << "Pre-filtering shards using fallback python minor version: "
                      << fallback_python_minor;
            return specs::Version::parse(std::string(fallback_python_minor)).value();
        }();

        auto maybe_load = load_channels(
            ctx,
            channel_context,
            db,
            package_caches,
            root_packages,
            python_minor_version_for_prefilter
        );

        if (!maybe_load)
        {
            throw maybe_load.error();
        }
        return { std::move(db), std::move(package_caches) };
    }

    PrefixData load_prefix_data_and_installed(
        Context& ctx,
        ChannelContext& channel_context,
        solver::libsolv::Database& db
    )
    {
        auto maybe_prefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
        if (!maybe_prefix_data)
        {
            throw std::runtime_error(maybe_prefix_data.error().what());
        }
        PrefixData prefix_data = std::move(maybe_prefix_data).value();
        load_installed_packages_in_database(ctx, db, prefix_data);
        return prefix_data;
    }

    bool handle_unsolvable_with_retry(
        solver::libsolv::Solver::Outcome& outcome,
        const Palette& palette,
        bool json_output,
        bool retry_clean_cache,
        bool is_retry,
        std::size_t& local_repodata_ttl,
        solver::libsolv::Database& db,
        const std::function<void()>& retry_fn,
        const std::function<void()>& pre_throw_hint
    )
    {
        auto* unsolvable = std::get_if<solver::libsolv::UnSolvable>(&outcome);
        if (unsolvable == nullptr)
        {
            return false;
        }
        if (!is_retry)
        {
            retry_fn();
            return true;
        }
        unsolvable->explain_problems_to(
            db,
            LOG_ERROR,
            {
                /* .unavailable= */ palette.failure,
                /* .available= */ palette.success,
            }
        );
        if (retry_clean_cache && !is_retry)
        {
            local_repodata_ttl = 2;
            retry_fn();
            return true;
        }
        if (pre_throw_hint)
        {
            pre_throw_hint();
        }
        write_unsolvable_json_if_needed(json_output, db, *unsolvable);
        throw mamba_error(
            "Could not solve for environment specs",
            mamba_error_code::satisfiablitity_error
        );
    }

    void write_unsolvable_json_if_needed(
        bool json_output,
        solver::libsolv::Database& db,
        solver::libsolv::UnSolvable& unsolvable
    )
    {
        if (json_output)
        {
            Console::instance().json_write(
                { { "success", false }, { "solver_problems", unsolvable.problems(db) } }
            );
        }
    }

    bool execute_transaction(
        MTransaction& transaction,
        Context& ctx,
        ChannelContext& channel_context,
        PrefixData& prefix_data,
        const std::function<void()>& before_execute,
        const std::function<void()>& after_execute,
        const std::function<void()>& on_abort
    )
    {
        if (ctx.output_params.json)
        {
            transaction.log_json();
        }
        const auto should_execute = transaction.prompt(ctx, channel_context);
        if (should_execute)
        {
            if (before_execute)
            {
                before_execute();
            }
            transaction.execute(ctx, channel_context, prefix_data);
            if (after_execute)
            {
                after_execute();
            }
        }
        else if (on_abort)
        {
            on_abort();
        }
        return should_execute;
    }

    MTransaction make_transaction_from_solution(
        Context& ctx,
        solver::libsolv::Database db,
        const solver::Request& request,
        const solver::libsolv::Solver::Outcome& outcome,
        MultiPackageCache& package_caches
    )
    {
        return MTransaction(ctx, db, request, std::get<solver::Solution>(outcome), package_caches);
    }

    void execute_other_pkg_managers(
        const std::vector<detail::other_pkg_mgr_spec>& other_specs,
        const Context& ctx,
        pip::Update update
    )
    {
        for (const auto& other_spec : other_specs)
        {
            auto result = install_for_other_pkgmgr(ctx, other_spec, update);
            if (!result)
            {
                static_assert(std::is_base_of_v<std::exception, decltype(result)::error_type>);
                throw std::move(result).error();
            }
        }
    }

    void execute_other_pkg_managers_if_needed(
        bool transaction_accepted,
        bool dry_run,
        const std::vector<detail::other_pkg_mgr_spec>& other_specs,
        const Context& ctx,
        pip::Update update
    )
    {
        if (transaction_accepted && !dry_run)
        {
            execute_other_pkg_managers(other_specs, ctx, update);
        }
    }

    std::optional<specs::Version>
    extract_requested_python_minor(const std::vector<std::string>& specs)
    {
        for (const auto& spec : specs)
        {
            auto maybe_name = specs::MatchSpec::extract_name(spec);
            if (!maybe_name.has_value() || maybe_name.value() != "python")
            {
                continue;
            }
            auto maybe_ms = specs::MatchSpec::parse(spec);
            if (!maybe_ms.has_value())
            {
                continue;
            }
            const specs::VersionSpec relaxed = relax_version_spec_to_minor(maybe_ms.value().version());
            if (auto maybe_v = version_from_single_equality_spec(relaxed))
            {
                return maybe_v;
            }
        }
        return std::nullopt;
    }

}
