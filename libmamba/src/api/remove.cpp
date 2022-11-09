// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/remove.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/core/transaction.hpp"

namespace mamba
{
    void remove(int flags)
    {
        bool prune = flags & MAMBA_REMOVE_PRUNE;
        bool force = flags & MAMBA_REMOVE_FORCE;
        bool remove_all = flags & MAMBA_REMOVE_ALL;

        auto& ctx = Context::instance();
        auto& config = Configuration::instance();

        config.at("use_target_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_NOT_ALLOW_MISSING_PREFIX
                       | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_EXPECT_EXISTING_PREFIX);
        config.load();

        auto remove_specs = config.at("specs").value<std::vector<std::string>>();

        if (remove_all)
        {
            auto sprefix_data = PrefixData::create(ctx.target_prefix);
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
            detail::remove_specs(remove_specs, prune, force);
        }
        else
        {
            Console::instance().print("Nothing to do.");
        }

        config.operation_teardown();
    }

    namespace detail
    {
        void remove_specs(const std::vector<std::string>& specs, bool prune, bool force)
        {
            auto& ctx = Context::instance();

            if (ctx.target_prefix.empty())
            {
                LOG_ERROR << "No active target prefix.";
                throw std::runtime_error("Aborted.");
            }

            auto exp_prefix_data = PrefixData::create(ctx.target_prefix);
            if (!exp_prefix_data)
            {
                // TODO: propagate tl::expected mechanism
                throw std::runtime_error(exp_prefix_data.error().what());
            }
            PrefixData& prefix_data = exp_prefix_data.value();

            MPool pool;
            MRepo::create(pool, prefix_data);

            const fs::u8path pkgs_dirs(ctx.root_prefix / "pkgs");
            MultiPackageCache package_caches({ pkgs_dirs });

            auto execute_transaction = [&](MTransaction& transaction)
            {
                if (ctx.json)
                    transaction.log_json();

                if (transaction.prompt())
                    transaction.execute(prefix_data);
            };

            if (force)
            {
                std::vector<MatchSpec> mspecs(specs.begin(), specs.end());
                auto transaction = MTransaction(pool, mspecs, {}, package_caches);
                execute_transaction(transaction);
            }
            else
            {
                MSolver solver(std::move(pool),
                               { { SOLVER_FLAG_ALLOW_DOWNGRADE, 1 },
                                 { SOLVER_FLAG_ALLOW_UNINSTALL, 1 },
                                 { SOLVER_FLAG_STRICT_REPO_PRIORITY,
                                   ctx.channel_priority == ChannelPriority::kStrict } });

                History history(ctx.target_prefix);
                auto hist_map = history.get_requested_specs_map();
                std::vector<std::string> keep_specs;
                for (auto& it : hist_map)
                    keep_specs.push_back(it.second.conda_build_form());

                solver.add_jobs(keep_specs, SOLVER_USERINSTALLED);

                int solver_flag = SOLVER_ERASE;

                if (prune)
                    solver_flag |= SOLVER_CLEANDEPS;

                solver.add_jobs(specs, solver_flag);
                solver.must_solve();

                MTransaction transaction(solver, package_caches);
                execute_transaction(transaction);
            }
        }
    }
}  // mamba
