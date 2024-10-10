// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include "mamba/api/channel_loader.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/update.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/pinning.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/solver.hpp"
#include "mamba/solver/request.hpp"

#include "pip_utils.hpp"

namespace mamba
{
    namespace
    {
        using command_args = std::vector<std::string>;

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

        // `env update` case
        if (update_params.env_update == EnvUpdate::Yes)
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
        config.load();

        const auto& raw_update_specs = config.at("specs").value<std::vector<std::string>>();

        auto channel_context = ChannelContext::make_conda_compatible(ctx);

        // add channels from specs
        for (const auto& s : raw_update_specs)
        {
            if (auto ms = specs::MatchSpec::parse(s); ms && ms->channel().has_value())
            {
                // Only register channels in the context once
                // NOTE: ctx.channels could be a `std::unordered_set` but `yaml-cpp` does not
                // support it. See: https://github.com/jbeder/yaml-cpp/issues/1322 Hence we
                // perform linear scanning: `ctx.channels` is a short `std::vector` in practice
                // so linearly searching is reasonable (probably even faster than using an
                // `std::unordered_set`).
                auto channel_name = ms->channel()->str();
                auto channel_is_absent = std::find(ctx.channels.begin(), ctx.channels.end(), channel_name)
                                         == ctx.channels.end();
                if (channel_is_absent)
                {
                    ctx.channels.push_back(channel_name);
                }
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
            install_for_other_pkgmgr(ctx, other_spec, pip::Update::Yes);
        }
    }
}
