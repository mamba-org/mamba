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
#include "mamba/solver/solver_factory.hpp"

#include "utils.hpp"

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
                            // MatchSpec.name().to_string() in `hist_map` second elements wouldn't
                            // be relevant
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
                                request.jobs.emplace_back(Request::Remove{
                                    specs::MatchSpec::parse(it.second.name().to_string())
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

        populate_context_channels_from_specs(raw_update_specs, ctx);

        solver::DatabaseVariant db_variant = ctx.experimental_resolvo_solver
                                                 ? solver::DatabaseVariant(
                                                       std::in_place_type<solver::resolvo::Database>,
                                                       channel_context.params()
                                                   )
                                                 : solver::DatabaseVariant(solver::libsolv::Database{
                                                       channel_context.params(),
                                                       {
                                                           ctx.experimental_matchspec_parsing
                                                               ? solver::libsolv::MatchSpecParser::Mamba
                                                               : solver::libsolv::MatchSpecParser::Libsolv,
                                                       } });

        if (!ctx.experimental_resolvo_solver)
        {
            add_spdlog_logger_to_database(std::get<solver::libsolv::Database>(db_variant));
        }

        MultiPackageCache package_caches(ctx.pkgs_dirs, ctx.validation_params);

        auto exp_loaded = load_channels(ctx, channel_context, db_variant, package_caches);
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

        load_installed_packages_in_database(
            ctx,
            std::visit(
                [](auto& db) -> std::variant<
                                 std::reference_wrapper<solver::libsolv::Database>,
                                 std::reference_wrapper<solver::resolvo::Database>>
                { return std::ref(db); },
                db_variant
            ),
            prefix_data
        );

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

        using LibsolvOutcome = std::variant<mamba::solver::Solution, mamba::solver::libsolv::UnSolvable>;
        auto outcome = ctx.experimental_resolvo_solver
                           ? solver::resolvo::Solver()
                                 .solve(std::get<solver::resolvo::Database>(db_variant), request)
                                 .map(
                                     [](auto&& result) -> LibsolvOutcome
                                     {
                                         // resolvo only returns Solution
                                         return std::get<mamba::solver::Solution>(result);
                                     }
                                 )
                           : solver::libsolv::Solver().solve(
                                 std::get<solver::libsolv::Database>(db_variant),
                                 request,
                                 ctx.experimental_matchspec_parsing
                                     ? solver::libsolv::MatchSpecParser::Mamba
                                     : solver::libsolv::MatchSpecParser::Libsolv
                             );

        if (!outcome.has_value())
        {
            throw std::runtime_error(outcome.error().what());
        }
        auto& result = outcome.value();
        if (auto* unsolvable = std::get_if<solver::libsolv::UnSolvable>(&result))
        {
            unsolvable->explain_problems_to(
                std::get<solver::libsolv::Database>(db_variant),
                LOG_ERROR,
                {
                    /* .unavailable= */ ctx.graphics_params.palette.failure,
                    /* .available= */ ctx.graphics_params.palette.success,
                }
            );
            if (ctx.output_params.json)
            {
                Console::instance().json_write(nlohmann::json{
                    { "success", false },
                    { "solver_problems",
                      unsolvable->problems(std::get<solver::libsolv::Database>(db_variant)) } });
            }
            throw mamba_error(
                "Could not solve for environment specs",
                mamba_error_code::satisfiablitity_error
            );
        }

        Console::instance().json_write(nlohmann::json{ { "success", true } });
        auto transaction = MTransaction(
            ctx,
            db_variant,
            request,
            std::get<solver::Solution>(result),
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
