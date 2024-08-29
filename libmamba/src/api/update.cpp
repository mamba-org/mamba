// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <reproc++/run.hpp>
#include <reproc/reproc.h>

#include "mamba/api/channel_loader.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/update.hpp"
#include "mamba/core/activation.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/pinning.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/solver.hpp"
#include "mamba/solver/request.hpp"

namespace mamba
{
    namespace
    {
        using command_args = std::vector<std::string>;

        tl::expected<command_args, std::runtime_error> get_other_pkg_mgr_update_instructions(
            const std::string& name,
            const std::string& target_prefix,
            const fs::u8path& spec_file
        )
        {
            const auto get_python_path = [&]
            { return util::which_in("python", get_path_dirs(target_prefix)).string(); };

            const std::unordered_map<std::string, command_args> other_pkg_mgr_update_instructions{
                { "pip",
                  { get_python_path(), "-m", "pip", "install", "-U", "-r", spec_file, "--no-input" } },
                { "pip --no-deps",
                  { get_python_path(), "-m", "pip", "install", "-U", "--no-deps", "-r", spec_file, "--no-input" } }
            };

            auto found_it = other_pkg_mgr_update_instructions.find(name);
            if (found_it != other_pkg_mgr_update_instructions.end())
            {
                return found_it->second;
            }
            else
            {
                return tl::unexpected(std::runtime_error(
                    fmt::format("no update instruction found for package manager '{}'", name)
                ));
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

        auto update_for_other_pkgmgr(const Context& ctx, const detail::other_pkg_mgr_spec& other_spec)
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

            command_args update_instructions = [&]
            {
                const auto maybe_instructions = get_other_pkg_mgr_update_instructions(
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
                update_instructions
            );

            reproc::options options;
            options.redirect.parent = true;
            options.working_directory = cwd.c_str();

            Console::stream() << fmt::format(
                ctx.graphics_params.palette.external,
                "\nUpdating {} packages: {}",
                pkg_mgr,
                fmt::join(deps, ", ")
            );
            fmt::print(LOG_INFO, "Calling: {}", fmt::join(update_instructions, " "));

            auto [status, ec] = reproc::run(wrapped_command, options);
            assert_reproc_success(options, status, ec);
            if (status != 0)
            {
                throw std::runtime_error("pip failed to update packages");
            }
        }

        auto create_update_request(
            PrefixData& prefix_data,
            std::vector<std::string> specs,
            const UpdateParams& update_params
        ) -> solver::Request
        {
            using Request = solver::Request;

            auto request = Request();

            if (update_params.update_all == UpdateAll::Yes)
            {
                if (update_params.prune_deps == PruneDeps::Yes)
                {
                    auto hist_map = prefix_data.history().get_requested_specs_map();
                    request.jobs.reserve(hist_map.size() + 1);

                    for (auto& [name, spec] : hist_map)
                    {
                        request.jobs.emplace_back(Request::Keep{ std::move(spec) });
                    }
                    request.jobs.emplace_back(Request::UpdateAll{ /* .clean_dependencies= */ true });
                }
                else
                {
                    request.jobs.emplace_back(Request::UpdateAll{ /* .clean_dependencies= */ false });
                }
            }
            else
            {
                request.jobs.reserve(specs.size());
                if (update_params.env_update == EnvUpdate::Yes)
                {
                    if (update_params.remove_not_specified == RemoveNotSpecified::Yes)
                    {
                        auto hist_map = prefix_data.history().get_requested_specs_map();
                        for (auto& it : hist_map)
                        {
                            // We use `spec_names` here because `specs` contain more info than just
                            // the spec name.
                            // Therefore, the search later and comparison (using `specs`) with
                            // MatchSpec.name().str() in `hist_map` second elements wouldn't be
                            // relevant
                            std::vector<std::string> spec_names;
                            spec_names.reserve(specs.size());
                            std::transform(
                                specs.begin(),
                                specs.end(),
                                std::back_inserter(spec_names),
                                [](const std::string& spec)
                                {
                                    return specs::MatchSpec::parse(spec)
                                        .or_else([](specs::ParseError&& err)
                                                 { throw std::move(err); })
                                        .value()
                                        .name()
                                        .str();
                                }
                            );

                            if (std::find(spec_names.begin(), spec_names.end(), it.second.name().str())
                                == spec_names.end())
                            {
                                request.jobs.emplace_back(Request::Remove{
                                    specs::MatchSpec::parse(it.second.name().str())
                                        .or_else([](specs::ParseError&& err)
                                                 { throw std::move(err); })
                                        .value(),
                                    /* .clean_dependencies= */ true,
                                });
                            }
                        }
                    }

                    // Install/update everything in specs
                    for (const auto& raw_ms : specs)
                    {
                        request.jobs.emplace_back(Request::Install{
                            specs::MatchSpec::parse(raw_ms)
                                .or_else([](specs::ParseError&& err) { throw std::move(err); })
                                .value(),
                        });
                    }
                }
                else
                {
                    for (const auto& raw_ms : specs)
                    {
                        request.jobs.emplace_back(Request::Update{
                            specs::MatchSpec::parse(raw_ms)
                                .or_else([](specs::ParseError&& err) { throw std::move(err); })
                                .value(),
                        });
                    }
                }
            }

            return request;
        }
    }

    void update(Configuration& config, const UpdateParams& update_params)
    {
        auto& ctx = config.context();

        config.at("use_target_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(
                MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_NOT_ALLOW_MISSING_PREFIX
                | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_EXPECT_EXISTING_PREFIX
            );
        config.load();

        const auto& raw_update_specs = config.at("specs").value<std::vector<std::string>>();

        auto channel_context = ChannelContext::make_conda_compatible(ctx);

        // add channels from specs
        for (const auto& s : raw_update_specs)
        {
            if (auto ms = specs::MatchSpec::parse(s); ms && ms->channel().has_value())
            {
                ctx.channels.push_back(ms->channel()->str());
            }
        }

        solver::libsolv::Database db{ channel_context.params() };
        add_spdlog_logger_to_database(db);

        MultiPackageCache package_caches(ctx.pkgs_dirs, ctx.validation_params);

        auto exp_loaded = load_channels(ctx, channel_context, db, package_caches);
        if (!exp_loaded)
        {
            throw std::runtime_error(exp_loaded.error().what());
        }

        auto exp_prefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
        if (!exp_prefix_data)
        {
            // TODO: propagate tl::expected mechanism
            throw std::runtime_error(exp_prefix_data.error().what());
        }
        PrefixData& prefix_data = exp_prefix_data.value();

        std::vector<std::string> prefix_pkgs;
        for (auto& it : prefix_data.records())
        {
            prefix_pkgs.push_back(it.first);
        }

        load_installed_packages_in_database(ctx, db, prefix_data);

        auto request = create_update_request(prefix_data, raw_update_specs, update_params);
        add_pins_to_request(
            request,
            ctx,
            prefix_data,
            raw_update_specs,
            /* no_pin= */ config.at("no_pin").value<bool>(),
            /* no_py_pin = */ config.at("no_py_pin").value<bool>()
        );

        request.flags = ctx.solver_flags;

        {
            auto out = Console::stream();
            print_request_pins_to(request, out);
            // Console stream prints on destruction
        }

        auto outcome = solver::libsolv::Solver().solve(db, request).value();
        if (auto* unsolvable = std::get_if<solver::libsolv::UnSolvable>(&outcome))
        {
            if (ctx.output_params.json)
            {
                Console::instance().json_write({ { "success", false },
                                                 { "solver_problems", unsolvable->problems(db) } });
            }
            throw mamba_error(
                "Could not solve for environment specs",
                mamba_error_code::satisfiablitity_error
            );
        }

        Console::instance().json_write({ { "success", true } });
        auto transaction = MTransaction(
            ctx,
            db,
            request,
            std::get<solver::Solution>(outcome),
            package_caches
        );


        auto execute_transaction = [&](MTransaction& trans)
        {
            if (ctx.output_params.json)
            {
                trans.log_json();
            }

            bool yes = trans.prompt(ctx, channel_context);
            if (yes)
            {
                trans.execute(ctx, channel_context, prefix_data);
            }
        };

        execute_transaction(transaction);
        for (auto other_spec :
             config.at("others_pkg_mgrs_specs").value<std::vector<detail::other_pkg_mgr_spec>>())
        {
            update_for_other_pkgmgr(ctx, other_spec);
        }
    }
}
