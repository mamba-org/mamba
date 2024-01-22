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
#include "mamba/core/pool.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"

namespace mamba
{
    void remove(Configuration& config, int flags)
    {
        auto& ctx = config.context();

        bool prune = flags & MAMBA_REMOVE_PRUNE;
        bool force = flags & MAMBA_REMOVE_FORCE;
        bool remove_all = flags & MAMBA_REMOVE_ALL;

        config.at("use_target_prefix_fallback").set_value(true);
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
                throw std::runtime_error("could not load prefix data");
            }
            PrefixData& prefix_data = sprefix_data.value();
            for (const auto& package : prefix_data.records())
            {
                remove_specs.push_back(package.second.name);
            }
        }

        if (!remove_specs.empty())
        {
            detail::remove_specs(ctx, channel_context, remove_specs, prune, force);
        }
        else
        {
            Console::instance().print("Nothing to do.");
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
        ) -> Request
        {
            auto request = Request();
            request.items.reserve(raw_specs.size());

            if (prune)
            {
                History history(ctx.prefix_params.target_prefix, channel_context);
                auto hist_map = history.get_requested_specs_map();

                request.items.reserve(request.items.capacity() + hist_map.size());

                for (auto& [name, spec] : hist_map)
                {
                    request.items.emplace_back(Request::Keep{ std::move(spec) });
                }
            }

            for (const auto& s : raw_specs)
            {
                request.items.emplace_back(Request::Remove{
                    specs::MatchSpec::parse(s),
                    /* .clean_dependencies= */ prune,
                });
            }

            return request;
        }
    }

    namespace detail
    {
        void remove_specs(
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

            MPool pool{ ctx, channel_context };

            load_installed_packages_in_pool(ctx, pool, prefix_data);

            const fs::u8path pkgs_dirs(ctx.prefix_params.root_prefix / "pkgs");
            MultiPackageCache package_caches({ pkgs_dirs }, ctx.validation_params);

            auto execute_transaction = [&](MTransaction& transaction)
            {
                if (ctx.output_params.json)
                {
                    transaction.log_json();
                }

                if (transaction.prompt())
                {
                    transaction.execute(prefix_data);
                }
            };

            if (force)
            {
                std::vector<specs::MatchSpec> mspecs;
                mspecs.reserve(raw_specs.size());
                std::transform(
                    raw_specs.begin(),
                    raw_specs.end(),
                    std::back_inserter(mspecs),
                    [&](const auto& spec_str) { return specs::MatchSpec::parse(spec_str); }
                );
                auto transaction = MTransaction(pool, mspecs, {}, package_caches);
                execute_transaction(transaction);
            }
            else
            {
                MSolver solver(
                    pool,
                    {
                        { SOLVER_FLAG_ALLOW_DOWNGRADE, 1 },
                        { SOLVER_FLAG_ALLOW_UNINSTALL, 1 },
                        { SOLVER_FLAG_STRICT_REPO_PRIORITY,
                          ctx.channel_priority == ChannelPriority::Strict },
                    }
                );

                solver.add_request(build_remove_request(ctx, channel_context, raw_specs, prune));

                solver.must_solve();

                MTransaction transaction(pool, solver, package_caches);
                execute_transaction(transaction);
            }
        }
    }
}
