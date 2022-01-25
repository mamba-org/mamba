// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"
#include "mamba/api/update.hpp"
#include "mamba/api/channel_loader.hpp"

#include "mamba/core/pinning.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/virtual_packages.hpp"


namespace mamba
{
    void update(bool update_all, bool prune)
    {
        auto& ctx = Context::instance();
        auto& config = Configuration::instance();

        config.at("use_target_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_NOT_ALLOW_MISSING_PREFIX
                       | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_EXPECT_EXISTING_PREFIX);
        config.load();

        auto update_specs = config.at("specs").value<std::vector<std::string>>();

        int solver_flag = SOLVER_UPDATE;

        MPool pool;
        MultiPackageCache package_caches(ctx.pkgs_dirs);

        auto repos = load_channels(pool, package_caches, 0);

        PrefixData prefix_data(ctx.target_prefix);
        prefix_data.load();

        std::vector<std::string> prefix_pkgs;
        for (auto& it : prefix_data.m_package_records)
            prefix_pkgs.push_back(it.first);

        prefix_data.add_virtual_packages(get_virtual_packages());

        auto repo = MRepo(pool, prefix_data);
        repos.push_back(repo);

        MSolver solver(pool,
                       { { SOLVER_FLAG_ALLOW_DOWNGRADE, ctx.allow_downgrade },
                         { SOLVER_FLAG_ALLOW_UNINSTALL, ctx.allow_uninstall },
                         { SOLVER_FLAG_STRICT_REPO_PRIORITY,
                           ctx.channel_priority == ChannelPriority::kStrict } });

        if (update_all)
        {
            PrefixData prefix_data(ctx.target_prefix);
            prefix_data.load();

            auto hist_map = prefix_data.history().get_requested_specs_map();
            std::vector<std::string> keep_specs;
            for (auto& it : hist_map)
            {
                keep_specs.push_back(it.second.name);
            }
            solver_flag |= SOLVER_SOLVABLE_ALL;
            if (prune)
            {
                solver_flag |= SOLVER_CLEANDEPS;
            }
            solver.add_jobs(keep_specs, SOLVER_USERINSTALLED);
            solver.add_global_job(solver_flag);
        }
        else
        {
            solver.add_jobs(update_specs, solver_flag);
        }

        auto& no_pin = config.at("no_pin").value<bool>();
        auto& no_py_pin = config.at("no_py_pin").value<bool>();

        if (!no_pin)
        {
            solver.add_pins(file_pins(prefix_data.path() / "conda-meta" / "pinned"));
            solver.add_pins(ctx.pinned_packages);
        }

        if (!no_py_pin)
        {
            auto py_pin = python_pin(prefix_data, update_specs);
            if (!py_pin.empty())
            {
                solver.add_pin(py_pin);
            }
        }
        if (!solver.pinned_specs().empty())
        {
            std::vector<std::string> pinned_str;
            for (auto& ms : solver.pinned_specs())
                pinned_str.push_back("  - " + ms.conda_build_form() + "\n");
            Console::print("\nPinned packages:\n" + join("", pinned_str));
        }

        solver.solve();

        MTransaction transaction(solver, package_caches);

        // TODO this is not so great
        std::vector<MRepo*> repo_ptrs;
        for (auto& r : repos)
        {
            repo_ptrs.push_back(&r);
        }

        auto execute_transaction = [&](MTransaction& transaction) {
            if (ctx.json)
                transaction.log_json();

            bool yes = transaction.prompt(repo_ptrs);
            if (yes)
                transaction.execute(prefix_data);
        };

        execute_transaction(transaction);

        config.operation_teardown();
    }
}
