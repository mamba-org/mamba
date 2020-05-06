// #include <cxxopts.hpp>
#include <CLI/CLI.hpp>

#include "activation.hpp"
#include "context.hpp"
#include "repo.hpp"
#include "transaction.hpp"
#include "prefix_data.hpp"
#include "subdirdata.hpp"
#include "solver.hpp"
#include "shell_init.hpp"

using namespace mamba;

static struct {
    bool hook;
    std::string shell_type;
    std::string action;
    std::string prefix = "base";
    bool stack;
} shell_options;

static struct {
    std::vector<std::string> specs;
    std::string prefix;
    std::string name;
} create_options;

static struct {
    int verbosity = 0;
} global_options;

void init_shell_parser(CLI::App* subcom)
{
    subcom->add_option("-s,--shell", shell_options.shell_type, "A shell type (bash, fish, posix, powershell)");
    subcom->add_option("--stack", shell_options.stack, 
        "Stack the environment being activated on top of the previous active environment, "
        "rather replacing the current active environment with a new one. Currently, "
        "only the PATH environment variable is stacked. "
        "This may be enabled implicitly by the 'auto_stack' configuration variable."
    );

    subcom->add_option("action", shell_options.action, "activate, deactivate or hook");
    // TODO add custom validator here!
    subcom->add_option("-p,--prefix", shell_options.prefix, 
                       "The root prefix to configure (for init and hook), and the prefix"
                       "to activate for activate, either by name or by path");

    subcom->callback([&]() {
        auto activator = mamba::PosixActivator();
        if (shell_options.action == "init")
        {
            init_shell(shell_options.shell_type, shell_options.prefix);
        }
        else if (shell_options.action == "hook")
        {
            Context::instance().root_prefix = shell_options.prefix;
            std::cout << activator.hook();
        }
        else if (shell_options.action == "activate")
        {
            if (shell_options.prefix == "base")
            {
                shell_options.prefix = Context::instance().root_prefix;
            }
            std::cout << activator.activate(shell_options.prefix, shell_options.stack);
        }
        else if (shell_options.action == "reactivate")
        {
            std::cout << activator.reactivate();
        }
        else if (shell_options.action == "deactivate")
        {
            std::cout << activator.deactivate();
        }
        else
        {
            throw std::runtime_error("Need an action (activate, deactivate or hook)");
        }
    });
}

void install_specs(const std::vector<std::string>& specs)
{
    auto& ctx = Context::instance();
    ctx.set_verbosity(global_options.verbosity);

    if (ctx.target_prefix.empty())
    {
        throw std::runtime_error("No active prefix (run `mamba activate`).");
    }
    if (!fs::exists(ctx.target_prefix))
    {
        throw std::runtime_error("Prefix does not exist exists");
    }

    fs::path cache_dir = ctx.root_prefix / "pkgs" / "cache";
    fs::create_directories(cache_dir);

    MSubdirData cfl("conda-forge",
                    "https://conda.anaconda.org/conda-forge/linux-64/repodata.json",
                    cache_dir / "cf_linux64.json");
    MSubdirData cfn("conda-forge",
                    "https://conda.anaconda.org/conda-forge/noarch/repodata.json",
                    cache_dir / "cf_noarch.json");
    cfl.load();
    cfn.load();
    MultiDownloadTarget multi_dl;
    multi_dl.add(cfl.target());
    multi_dl.add(cfn.target());
    multi_dl.download(true);

    std::vector<MRepo*> repos;
    MPool pool;
    PrefixData prefix_data(ctx.target_prefix);
    prefix_data.load();
    auto repo = MRepo(pool, prefix_data);
    repos.push_back(&repo);

    MRepo cfl_r(pool, "conda-forge/linux-64", cfl.cache_path(), "https://conda.anaconda.org/conda-forge/linux-64/");
    cfl_r.set_priority(0, 1);
    repos.push_back(&cfl_r);

    MRepo cfl_n(pool, "conda-forge/noarch", cfn.cache_path(), "https://conda.anaconda.org/conda-forge/noarch/");
    cfl_n.set_priority(0, 0);
    repos.push_back(&cfl_n);

    MSolver solver(pool, {{SOLVER_FLAG_ALLOW_DOWNGRADE, 1}});
    solver.add_jobs(create_options.specs, SOLVER_INSTALL);
    // solver.add_jobs({"xtensor"}, SOLVER_UPDATE);
    solver.solve();

    mamba::MTransaction trans(solver);
    trans.print();
    bool yes = trans.prompt(ctx.root_prefix / "pkgs", repos);
    if (!yes) exit(0);

    trans.execute(prefix_data, ctx.root_prefix / "pkgs");
}

void init_install_parser(CLI::App* subcom)
{
    subcom->add_option("specs", create_options.specs, "Specs to install into the new environment");
    subcom->callback([&]() {
        install_specs(create_options.specs);
    });
}


void init_create_parser(CLI::App* subcom)
{
    subcom->add_option("specs", create_options.specs, "Specs to install into the new environment");
    subcom->add_option("-p,--prefix", create_options.prefix, "Path to the Prefix");
    // subcom->add_option("-n,--name" create_options.name, "Prefix name");

    subcom->callback([&]() {
        auto& ctx = Context::instance();
        ctx.set_verbosity(global_options.verbosity);
        ctx.target_prefix = create_options.prefix;

        if (fs::exists(ctx.target_prefix))
        {
            throw std::runtime_error("Prefix already exists");
        }
        else
        {
            fs::create_directories(ctx.target_prefix);
        }
        install_specs(create_options.specs);

        return 0;
    });
}

int main(int argc, char** argv)
{
    CLI::App app{"App description"};

    app.add_flag("-v", global_options.verbosity, "Enbable verbose mode (higher verbosity with multiple -v, e.g. -vvv)");

    CLI::App* shell_subcom = app.add_subcommand("shell", "Generate shell init scripts");
    init_shell_parser(shell_subcom);

    CLI::App* create_subcom = app.add_subcommand("create", "Create new environment");
    init_create_parser(create_subcom);

    CLI::App* install_subcom = app.add_subcommand("install", "Install packages in active");
    init_install_parser(install_subcom);

    CLI11_PARSE(app, argc, argv);

    return 0;
}
