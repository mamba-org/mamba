// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cctype>

#include <fmt/format.h>

#include "mamba/api/channel_loader.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"
#include "mamba/api/update.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/util_scope.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/request.hpp"
#include "mamba/specs/match_spec.hpp"

#include "utils.hpp"

namespace mamba
{
    namespace
    {
        auto create_update_request(
            PrefixData& prefix_data,
            std::vector<std::string> specs,
            const UpdateParams& update_params
        ) -> solver::Request
        {
            using Request = solver::Request;

            auto request = Request();

            // Parse all specs upfront
            std::vector<specs::MatchSpec> parsed_specs;
            parsed_specs.reserve(specs.size());
            for (const auto& raw_ms : specs)
            {
                parsed_specs.push_back(
                    specs::MatchSpec::parse(raw_ms)
                        .or_else([](specs::ParseError&& err) { throw std::move(err); })
                        .value()
                );
            }

            if (update_params.update_all == UpdateAll::Yes)
            {
                if (update_params.prune_deps == PruneDeps::Yes)
                {
                    auto hist_map = prefix_data.history().get_requested_specs_map();
                    request.jobs.reserve(hist_map.size() + specs.size() + 1);

                    for (auto& [name, spec] : hist_map)
                    {
                        request.jobs.emplace_back(Request::Keep{ std::move(spec) });
                    }
                    request.jobs.emplace_back(Request::UpdateAll{ /* .clean_dependencies= */ true });
                }
                else
                {
                    request.jobs.reserve(specs.size() + 1);
                    request.jobs.emplace_back(Request::UpdateAll{ /* .clean_dependencies= */ false });
                }

                // Install everything else
                for (auto& ms : parsed_specs)
                {
                    request.jobs.emplace_back(Request::Install{ std::move(ms) });
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
                        for (auto& [name, _] : hist_map)
                        {
                            auto it = std::find_if(
                                parsed_specs.begin(),
                                parsed_specs.end(),
                                [&](const auto& ms) { return ms.name().to_string() == name; }
                            );
                            if (it == parsed_specs.end())
                            {
                                request.jobs.emplace_back(
                                    Request::Remove{
                                        specs::MatchSpec::parse(name)
                                            .or_else([](specs::ParseError&& err)
                                                     { throw std::move(err); })
                                            .value(),
                                        /* .clean_dependencies= */ true,
                                    }
                                );
                            }
                        }
                    }

                    // Install/update everything in specs
                    for (auto& ms : parsed_specs)
                    {
                        request.jobs.emplace_back(Request::Install{ std::move(ms) });
                    }
                }
                else
                {
                    for (auto& ms : parsed_specs)
                    {
                        request.jobs.emplace_back(Request::Update{ std::move(ms) });
                    }
                }
            }

            return request;
        }

        void update_impl(
            Context& ctx,
            ChannelContext& channel_context,
            Configuration& config,
            const std::vector<std::string>& raw_update_specs,
            const UpdateParams& update_params,
            bool is_retry
        )
        {
            auto& no_pin = config.at("no_pin").value<bool>();
            auto& no_py_pin = config.at("no_py_pin").value<bool>();
            auto& retry_clean_cache = config.at("retry_clean_cache").value<bool>();

            // `update --all` is particularly sensitive to stale shard index snapshots because no
            // explicit specs constrain which package names must be present in the remote universe.
            // Force shard-index refresh for this command shape so shard mode sees the same update
            // candidates as flat repodata.
            const bool force_fresh_shard_index = update_params.update_all == UpdateAll::Yes
                                                 && ctx.use_sharded_repodata && !ctx.offline;
            const auto saved_shards_ttl = ctx.repodata_shards_ttl;
            on_scope_exit restore_shards_ttl{ [&] { ctx.repodata_shards_ttl = saved_shards_ttl; } };
            if (force_fresh_shard_index)
            {
                ctx.repodata_shards_ttl = 0;
            }

            validate_target_prefix_and_channels(ctx, /* create_env= */ false);
            auto [db, package_caches] = prepare_solver_context(
                ctx,
                channel_context,
                raw_update_specs,
                is_retry,
                no_py_pin
            );

            auto prefix_data = load_prefix_data_and_installed(ctx, channel_context, db);

            if (update_params.env_update == EnvUpdate::No && update_params.update_all == UpdateAll::No)
            {
                for (const auto& raw_spec : raw_update_specs)
                {
                    auto match_spec_name = specs::MatchSpec::parse(raw_spec)
                                               .or_else([](specs::ParseError&& err)
                                                        { throw std::move(err); })
                                               .value()
                                               .name()
                                               .to_string();
                    if (!prefix_data.records().contains(match_spec_name))
                    {
                        throw std::runtime_error(
                            fmt::format(
                                "Package is not installed in prefix.\n"
                                "prefix: {}\n"
                                "package name: {}\n"
                                "\n"
                                "Use `(micro)mamba env update` or `(micro)mamba update --all` to install new packages.",
                                prefix_data.path().string(),
                                match_spec_name
                            )
                        );
                    }
                }
            }

            auto request = create_update_request(prefix_data, raw_update_specs, update_params);

            add_pins_to_request(request, ctx, prefix_data, raw_update_specs, no_pin, no_py_pin);

            {
                auto out = Console::stream();
                print_request_pins_to(request, out);
                // Console stream prints on destruction
            }

            auto outcome = solve_request_with_status(ctx.experimental_matchspec_parsing, db, request);

            if (handle_unsolvable_with_retry(
                    outcome,
                    ctx.graphics_params.palette,
                    ctx.output_params.json,
                    retry_clean_cache,
                    is_retry,
                    ctx.local_repodata_ttl,
                    db,
                    /* retry_fn= */
                    [&]()
                    {
                        bool retry = true;
                        update_impl(ctx, channel_context, config, raw_update_specs, update_params, retry);
                    }
                ))
            {
                return;
            }

            std::vector<LockFile> locks;
            for (auto& c : ctx.pkgs_dirs)
            {
                locks.push_back(LockFile(c));
            }

            Console::instance().set_json_output_success(true);

            auto trans = make_transaction_from_solution(ctx, std::move(db), request, outcome, package_caches);

            Console::stream();

            auto transaction_accepted = execute_transaction(
                trans,
                ctx,
                channel_context,
                prefix_data,
                /* before_execute= */ {},
                /* after_execute= */
                [&]()
                {
                    if (update_params.env_update == EnvUpdate::Yes && !ctx.dry_run)
                    {
                        print_activation_message(ctx);
                    }
                },
                /* on_abort= */ {}
            );

            const auto other_specs = config.at("others_pkg_mgrs_specs")
                                         .value<std::vector<detail::other_pkg_mgr_spec>>();
            execute_other_pkg_managers_if_needed(
                transaction_accepted,
                ctx.dry_run,
                other_specs,
                ctx,
                pip::Update::Yes
            );
        }
    }

    void update(Configuration& config, const UpdateParams& update_params)
    {
        auto& ctx = config.context();

        // `env update` case
        configure_common_prefix_fallbacks(
            config,
            /* create_base= */ update_params.env_update == EnvUpdate::Yes
        );
        config.load();

        const auto& raw_update_specs = config.at("specs").value<std::vector<std::string>>();
        auto channel_context = ChannelContext::make_conda_compatible(ctx);

        auto is_retry = false;
        update_impl(ctx, channel_context, config, raw_update_specs, update_params, is_retry);
    }
}
