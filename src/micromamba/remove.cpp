// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "remove.hpp"
#include "info.hpp"
#include "common_options.hpp"

#include "mamba/configuration.hpp"
#include "mamba/output.hpp"
#include "mamba/package_cache.hpp"
#include "mamba/prefix_data.hpp"
#include "mamba/repo.hpp"
#include "mamba/solver.hpp"
#include "mamba/transaction.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_remove_parser(CLI::App* subcom)
{
    init_general_options(subcom);
    init_prefix_options(subcom);

    auto& config = Configuration::instance();

    auto& specs = config.insert(Configurable("remove_specs", std::vector<std::string>({}))
                                    .group("cli")
                                    .rc_configurable(false)
                                    .description("Specs to remove from the environment"));
    subcom->add_option("specs", specs.set_cli_config({}), specs.description());
}


void
set_remove_command(CLI::App* subcom)
{
    init_remove_parser(subcom);

    subcom->callback([&]() {
        load_configuration();

        auto& config = Configuration::instance();
        auto& specs = config.at("remove_specs").value<std::vector<std::string>>();

        if (!specs.empty())
        {
            check_target_prefix(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                                | MAMBA_ALLOW_EXISTING_PREFIX);
            remove_specs(specs);
        }
        else
        {
            Console::print("Nothing to do.");
        }
    });
}

void
remove_specs(const std::vector<std::string>& specs)
{
    auto& ctx = Context::instance();

    if (ctx.target_prefix.empty())
    {
        LOG_ERROR << "No active target prefix.";
        exit(1);
    }

    std::vector<MRepo> repos;
    MPool pool;
    PrefixData prefix_data(ctx.target_prefix);
    prefix_data.load();
    auto repo = MRepo(pool, prefix_data);
    repos.push_back(repo);

    MSolver solver(pool,
                   { { SOLVER_FLAG_ALLOW_DOWNGRADE, 1 }, { SOLVER_FLAG_ALLOW_UNINSTALL, 1 } });
    solver.add_jobs(specs, SOLVER_ERASE);
    solver.solve();

    MultiPackageCache package_caches({ ctx.root_prefix / "pkgs" });
    MTransaction trans(solver, package_caches);

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

    bool yes = trans.prompt(ctx.root_prefix / "pkgs", repo_ptrs);
    if (!yes)
        exit(0);

    trans.execute(prefix_data, ctx.root_prefix / "pkgs");
}
