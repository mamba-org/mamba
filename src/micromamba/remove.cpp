// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "remove.hpp"
#include "info.hpp"
#include "parsers.hpp"

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

    subcom->add_option("specs", create_options.specs, "Specs to remove from the environment");
}

void
load_remove_options(Context& ctx)
{
    load_general_options(ctx);
    load_prefix_options(ctx);
    load_rc_options(ctx);
}

void
set_remove_command(CLI::App* subcom)
{
    init_remove_parser(subcom);

    subcom->callback([&]() {
        load_remove_options(Context::instance());

        if (!create_options.specs.empty())
        {
            remove_specs(create_options.specs);
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
    load_general_options(ctx);

    Console::print(banner);

    if (ctx.target_prefix.empty())
    {
        throw std::runtime_error(
            "No active target prefix.\n\nRun $ micromamba activate <PATH_TO_MY_ENV>\nto activate an environment.\n");
    }

    std::vector<MRepo> repos;
    MPool pool;
    PrefixData prefix_data(ctx.target_prefix);
    prefix_data.load();
    auto repo = MRepo(pool, prefix_data);
    repos.push_back(repo);

    MSolver solver(pool,
                   { { SOLVER_FLAG_ALLOW_DOWNGRADE, 1 }, { SOLVER_FLAG_ALLOW_UNINSTALL, 1 } });
    solver.add_jobs(create_options.specs, SOLVER_ERASE);
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

    std::cout << std::endl;
    bool yes = trans.prompt(ctx.root_prefix / "pkgs", repo_ptrs);
    if (!yes)
        exit(0);

    trans.execute(prefix_data, ctx.root_prefix / "pkgs");
}
