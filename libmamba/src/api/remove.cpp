// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/remove.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/solver/libsolv/solver.hpp"
#include "mamba/solver/request.hpp"

namespace mamba
{
    RemoveResult remove(Configuration& config, int flags)
    {
        auto& ctx = config.context();

        bool prune = flags & MAMBA_REMOVE_PRUNE;
        bool force = flags & MAMBA_REMOVE_FORCE;
        bool remove_all = flags & MAMBA_REMOVE_ALL;

        config.at("use_target_prefix_fallback").set_value(true);
        config.at("use_default_prefix_fallback").set_value(false);
        config.at("use_root_prefix_fallback").set_value(false);
        config.at("target_prefix_checks")
            .set_value(
                MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_NOT_ALLOW_MISSING_PREFIX
                | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_EXPECT_EXISTING_PREFIX
            );
        config.load();

        auto remove_specs = config.at("specs").value<std::vector<std::string>>();

        auto channel_context = ChannelContext::make_conda_compatible(ctx);

        if (remove_all)
        {
            auto sprefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
            if (!sprefix_data)
            {
                // TODO: propagate tl::expected mechanism
                throw std::runtime_error(
                    fmt::format("could not load prefix data: {}", sprefix_data.error().what())
                );
            }
            PrefixData& prefix_data = sprefix_data.value();
            for (const auto& package : prefix_data.records())
            {
                remove_specs.push_back(package.second.name);
            }
        }

        if (!remove_specs.empty())
        {
            return detail::remove_specs(ctx, channel_context, remove_specs, prune, force)
                       ? RemoveResult::YES
                       : RemoveResult::NO;
        }
        else
        {
            Console::instance().print("Nothing to do.");
            return RemoveResult::EMPTY;
        }
    }

    namespace
    {
        template <typename Range>
        auto build_remove_request(
            const Context& ctx,
            ChannelContext& channel_context,
            const Range& raw_specs,
            bool prune
        ) -> solver::Request
        {
            using Request = solver::Request;

            auto request = Request();
            request.jobs.reserve(raw_specs.size());

            if (prune)
            {
                History history(ctx.prefix_params.target_prefix, channel_context);
                auto hist_map = history.get_requested_specs_map();

                request.jobs.reserve(request.jobs.capacity() + hist_map.size());

                for (auto& [name, spec] : hist_map)
                {
                    request.jobs.emplace_back(Request::Keep{ std::move(spec) });
                }
            }

            for (const auto& s : raw_specs)
            {
                request.jobs.emplace_back(Request::Remove{
                    specs::MatchSpec::parse(s)
                        .or_else([](specs::ParseError&& err) { throw std::move(err); })
                        .value(),
                    /* .clean_dependencies= */ prune,
                });
            }

            return request;
        }
    }

    namespace detail
    {
        bool remove_specs(
            Context& ctx,
            ChannelContext& channel_context,
            const std::vector<std::string>& raw_specs,
            bool prune,
            bool force
        )
        {
            if (ctx.prefix_params.target_prefix.empty())
            {
                LOG_ERROR << "No active target prefix.";
                throw std::runtime_error("Aborted.");
            }

            auto exp_prefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
            if (!exp_prefix_data)
            {
                // TODO: propagate tl::expected mechanism
                throw std::runtime_error(exp_prefix_data.error().what());
            }
            PrefixData& prefix_data = exp_prefix_data.value();

            solver::libsolv::Database database{
                channel_context.params(),
                {
                    ctx.experimental_matchspec_parsing ? solver::libsolv::MatchSpecParser::Mamba
                                                       : solver::libsolv::MatchSpecParser::Libsolv,
                },
            };
            add_spdlog_logger_to_database(database);
            load_installed_packages_in_database(ctx, database, prefix_data);

            const fs::u8path pkgs_dirs(ctx.prefix_params.root_prefix / "pkgs");
            MultiPackageCache package_caches({ pkgs_dirs }, ctx.validation_params);

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

            if (force)
            {
                std::vector<specs::PackageInfo> pkgs_to_remove;
                pkgs_to_remove.reserve(raw_specs.size());
                for (const auto& str : raw_specs)
                {
                    auto spec = specs::MatchSpec::parse(str)
                                    .or_else([](specs::ParseError&& err) { throw std::move(err); })
                                    .value();
                    const auto& installed = prefix_data.records();
                    // TODO should itreate over all packages and use MatchSpec.contains
                    // TODO should move such method over Pool for consistent use
                    if (auto iter = installed.find(spec.name().str()); iter != installed.cend())
                    {
                        pkgs_to_remove.push_back(iter->second);
                    }
                }
                auto transaction = MTransaction(ctx, database, pkgs_to_remove, {}, package_caches);
                return execute_transaction(transaction);
            }
            else
            {
                auto request = build_remove_request(ctx, channel_context, raw_specs, prune);
                request.flags = {
                    /* .keep_dependencies= */ true,
                    /* .keep_user_specs= */ true,
                    /* .force_reinstall= */ false,
                    /* .allow_downgrade= */ true,
                    /* .allow_uninstall= */ true,
                    /* .strict_repo_priority= */ ctx.channel_priority == ChannelPriority::Strict,
                };

                auto outcome = solver::libsolv::Solver()
                                   .solve(
                                       database,
                                       request,
                                       ctx.experimental_matchspec_parsing
                                           ? solver::libsolv::MatchSpecParser::Mamba
                                           : solver::libsolv::MatchSpecParser::Mixed
                                   )
                                   .value();
                if (auto* unsolvable = std::get_if<solver::libsolv::UnSolvable>(&outcome))
                {
                    if (ctx.output_params.json)
                    {
                        Console::instance().json_write({ { "success", false },
                                                         { "solver_problems",
                                                           unsolvable->problems(database) } });
                    }
                    throw mamba_error(
                        "Could not solve for environment specs",
                        mamba_error_code::satisfiablitity_error
                    );
                }

                Console::instance().json_write({ { "success", true } });
                auto transaction = MTransaction(
                    ctx,
                    database,
                    request,
                    std::get<solver::Solution>(outcome),
                    package_caches
                );

                return execute_transaction(transaction);
            }
        }
    }
}
