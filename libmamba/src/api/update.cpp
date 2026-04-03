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
                                        .to_string();
                                }
                            );

                            if (std::find(
                                    spec_names.begin(),
                                    spec_names.end(),
                                    it.second.name().to_string()
                                )
                                == spec_names.end())
                            {
                                request.jobs.emplace_back(
                                    Request::Remove{
                                        specs::MatchSpec::parse(it.second.name().to_string())
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
                    for (const auto& raw_ms : specs)
                    {
                        request.jobs.emplace_back(
                            Request::Install{
                                specs::MatchSpec::parse(raw_ms)
                                    .or_else([](specs::ParseError&& err) { throw std::move(err); })
                                    .value(),
                            }
                        );
                    }
                }
                else
                {
                    for (const auto& raw_ms : specs)
                    {
                        request.jobs.emplace_back(
                            Request::Update{
                                specs::MatchSpec::parse(raw_ms)
                                    .or_else([](specs::ParseError&& err) { throw std::move(err); })
                                    .value(),
                            }
                        );
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

            validate_target_prefix_and_channels(ctx, /* create_env= */ false);
            auto [db, package_caches] = prepare_solver_context(
                ctx,
                channel_context,
                raw_update_specs,
                is_retry,
                no_py_pin
            );

            auto prefix_data = load_prefix_data_and_installed(ctx, channel_context, db);

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

            Console::instance().json_write({ { "success", true } });

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
