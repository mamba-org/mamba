// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <solv/solver.h>

#include "mamba/api/channel_loader.hpp"
#include "mamba/api/configuration.hpp"
#include "mamba/api/update.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/pinning.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    void update(Configuration& config, bool update_all, bool prune_deps, bool remove_not_specified)
    {
        auto& ctx = config.context();

        config.at("use_target_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(
                MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_NOT_ALLOW_MISSING_PREFIX
                | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_EXPECT_EXISTING_PREFIX
            );
        config.load();

        auto update_specs = config.at("specs").value<std::vector<std::string>>();

        auto channel_context = ChannelContext::make_conda_compatible(ctx);

        // add channels from specs
        for (const auto& s : update_specs)
        {
            if (auto m = MatchSpec{ s, ctx, channel_context }; m.channel.has_value())
            {
                ctx.channels.push_back(m.channel->str());
            }
        }

        int solver_flag = SOLVER_UPDATE;

        MPool pool{ ctx, channel_context };
        MultiPackageCache package_caches(ctx.pkgs_dirs, ctx.validation_params);

        auto exp_loaded = load_channels(ctx, pool, package_caches, 0);
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

        prefix_data.add_packages(get_virtual_packages(ctx));

        MRepo(pool, prefix_data);

        MSolver solver(
            pool,
            {
                { SOLVER_FLAG_ALLOW_DOWNGRADE, ctx.allow_downgrade },
                { SOLVER_FLAG_ALLOW_UNINSTALL, ctx.allow_uninstall },
                { SOLVER_FLAG_STRICT_REPO_PRIORITY, ctx.channel_priority == ChannelPriority::Strict },
            }
        );

        auto& no_pin = config.at("no_pin").value<bool>();
        auto& no_py_pin = config.at("no_py_pin").value<bool>();

        if (!no_pin)
        {
            solver.add_pins(file_pins(prefix_data.path() / "conda-meta" / "pinned"));
            solver.add_pins(ctx.pinned_packages);
        }

        if (!no_py_pin)
        {
            auto py_pin = python_pin(ctx, channel_context, prefix_data, update_specs);
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

        if (update_all)
        {
            auto hist_map = prefix_data.history().get_requested_specs_map(ctx);
            std::vector<std::string> keep_specs;
            for (auto& it : hist_map)
            {
                keep_specs.push_back(it.second.name);
            }
            solver_flag |= SOLVER_SOLVABLE_ALL;
            if (prune_deps)
            {
                solver_flag |= SOLVER_CLEANDEPS;
            }
            solver.add_jobs(keep_specs, SOLVER_USERINSTALLED);
            solver.add_global_job(solver_flag);
        }
        else
        {
            if (remove_not_specified)
            {
                auto hist_map = prefix_data.history().get_requested_specs_map(ctx);
                std::vector<std::string> remove_specs;
                for (auto& it : hist_map)
                {
                    if (std::find(update_specs.begin(), update_specs.end(), it.second.name)
                        == update_specs.end())
                    {
                        remove_specs.push_back(it.second.name);
                    }
                }
                solver.add_jobs(remove_specs, SOLVER_ERASE | SOLVER_CLEANDEPS);
            }
            solver.add_jobs(update_specs, solver_flag);
        }


        solver.must_solve();

        auto execute_transaction = [&](MTransaction& transaction)
        {
            if (ctx.output_params.json)
            {
                transaction.log_json();
            }

            bool yes = transaction.prompt();
            if (yes)
            {
                transaction.execute(prefix_data);
            }
        };

        MTransaction transaction(pool, solver, package_caches);
        execute_transaction(transaction);
    }
}
