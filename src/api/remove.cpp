// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/remove.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/core/transaction.hpp"


namespace mamba
{
    void remove(bool remove_all)
    {
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
            PrefixData prefix_data(ctx.target_prefix);
            prefix_data.load();

            for (const auto& package : prefix_data.m_package_records)
            {
                remove_specs.push_back(package.second.name);
            }
        }

        if (!remove_specs.empty())
        {
            detail::remove_specs(remove_specs);
        }
        else
        {
            Console::print("Nothing to do.");
        }

        config.operation_teardown();
    }

    namespace detail
    {
        void remove_specs(const std::vector<std::string>& specs)
        {
            auto& ctx = Context::instance();

            if (ctx.target_prefix.empty())
            {
                LOG_ERROR << "No active target prefix.";
                throw std::runtime_error("Aborted.");
            }

            std::vector<MRepo> repos;
            MPool pool;
            PrefixData prefix_data(ctx.target_prefix);
            prefix_data.load();
            auto repo = MRepo(pool, prefix_data);
            repos.push_back(repo);

            MSolver solver(
                pool, { { SOLVER_FLAG_ALLOW_DOWNGRADE, 1 }, { SOLVER_FLAG_ALLOW_UNINSTALL, 1 } });

            History history(ctx.target_prefix);
            auto hist_map = history.get_requested_specs_map();
            std::vector<std::string> keep_specs;
            for (auto& it : hist_map)
                keep_specs.push_back(it.second.conda_build_form());

            solver.add_jobs(keep_specs, SOLVER_USERINSTALLED);

            solver.add_jobs(specs, SOLVER_ERASE | SOLVER_CLEANDEPS);
            solver.solve();

            const fs::path pkgs_dirs(ctx.root_prefix / "pkgs");
            MultiPackageCache package_caches({ pkgs_dirs });
            MTransaction trans(solver, package_caches, pkgs_dirs);

            if (ctx.json)
            {
                trans.log_json();
            }
            // TODO this is not so great
            std::vector<MRepo*> repo_ptrs;
            for (auto& r : repos)
            {
                repo_ptrs.push_back(&r);
            }

            bool yes = trans.prompt(repo_ptrs);
            if (yes)
                trans.execute(prefix_data);
        }
    }
}  // mamba
