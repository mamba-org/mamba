// #include <cxxopts.hpp>
#include <CLI/CLI.hpp>

#include "activation.hpp"
#include "channel.hpp"
#include "context.hpp"
#include "repo.hpp"
#include "transaction.hpp"
#include "prefix_data.hpp"
#include "subdirdata.hpp"
#include "solver.hpp"
#include "shell_init.hpp"

const char banner[] = R"MAMBARAW(
                                           __
          __  ______ ___  ____ _____ ___  / /_  ____ _
         / / / / __ `__ \/ __ `/ __ `__ \/ __ \/ __ `/
        / /_/ / / / / / / /_/ / / / / / / /_/ / /_/ /
       / .___/_/ /_/ /_/\__,_/_/ /_/ /_/_.___/\__,_/
      /_/
)MAMBARAW";

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
    std::vector<std::string> channels;
} create_options;

static struct {
    bool ssl_verify = true;
    std::string cacert_path;
} network_options;

static struct {
    int verbosity = 0;
    bool always_yes = false;
    bool quiet = false;
    bool json = false;
    bool offline = false;
} global_options;


void init_network_parser(CLI::App* subcom)
{
    subcom->add_option("--ssl_verify", network_options.ssl_verify, "Enable or disable SSL verification");
    subcom->add_option("--cacert_path", network_options.cacert_path, "Path for CA Certificate");
}

void init_global_parser(CLI::App* subcom)
{
    subcom->add_flag("-v,--verbose", global_options.verbosity, "Enbable verbose mode (higher verbosity with multiple -v, e.g. -vvv)");
    subcom->add_flag("-q,--quiet", global_options.quiet, "Quiet mode (print less output)");
    subcom->add_flag("-y,--yes", global_options.always_yes, "Automatically answer yes on all questions");
    subcom->add_flag("--json", global_options.json, "Report all output as json");
    subcom->add_flag("--offline", global_options.offline, "Force use cached repodata");
}

void set_network_options(Context& ctx)
{
    std::array<std::string, 6> cert_locations {
        "/etc/ssl/certs/ca-certificates.crt",                // Debian/Ubuntu/Gentoo etc.
        "/etc/pki/tls/certs/ca-bundle.crt",                  // Fedora/RHEL 6
        "/etc/ssl/ca-bundle.pem",                            // OpenSUSE
        "/etc/pki/tls/cacert.pem",                           // OpenELEC
        "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem", // CentOS/RHEL 7
        "/etc/ssl/cert.pem",                                 // Alpine Linux
    };

    // ssl verify can be either an empty string (regular SSL verification),
    // the string "<false>" to indicate no SSL verification, or a path to
    // a directory with cert files, or a cert file.
    if (network_options.ssl_verify == false)
    {
        ctx.ssl_verify = "<false>";
    }
    else if (!network_options.cacert_path.empty())
    {
        ctx.ssl_verify = network_options.cacert_path;
    }
    else
    {
        for (const auto& loc : cert_locations)
        {
            if (fs::exists(loc))
            {
                ctx.ssl_verify = loc;
            }
        }
        if (ctx.ssl_verify.empty())
        {
            LOG_WARNING << "No ca certificates found, disabling SSL verification";
            ctx.ssl_verify = "<false>";
        }
    }
}

void set_global_options(Context& ctx)
{
    ctx.set_verbosity(global_options.verbosity);
    ctx.quiet = global_options.quiet;
    ctx.json = global_options.json;
    ctx.always_yes = global_options.always_yes;
    ctx.offline = global_options.offline;
}

void init_channel_parser(CLI::App* subcom)
{
    subcom->add_option("-c,--channel", create_options.channels) \
          ->type_size(1, 1) \
          ->allow_extra_args(false);
}

void set_channels(Context& ctx)
{
    ctx.channels = create_options.channels;
}

void init_shell_parser(CLI::App* subcom)
{
    subcom->add_option("-s,--shell", shell_options.shell_type, "A shell type (bash, fish, posix, powershell, xonsh)");
    subcom->add_option("--stack", shell_options.stack,
        "Stack the environment being activated on top of the previous active environment, "
        "rather replacing the current active environment with a new one. Currently, "
        "only the PATH environment variable is stacked. "
        "This may be enabled implicitly by the 'auto_stack' configuration variable."
    );

    subcom->add_option("action", shell_options.action, "activate, deactivate or hook");
    // TODO add custom validator here!
    subcom->add_option("-p,--prefix", shell_options.prefix,
                       "The root prefix to configure (for init and hook), and the prefix "
                       "to activate for activate, either by name or by path");

    subcom->callback([&]() {
        std::unique_ptr<Activator> activator;
        if (shell_options.shell_type == "bash" || shell_options.shell_type == "zsh")
        {
            activator = std::make_unique<mamba::PosixActivator>();
        }
        else if (shell_options.shell_type == "cmd.exe")
        {
            activator = std::make_unique<mamba::CmdExeActivator>();
        }
        else if (shell_options.shell_type == "powershell")
        {
            activator = std::make_unique<mamba::PowerShellActivator>();
        }
        else if (shell_options.shell_type == "xonsh")
        {
            activator = std::make_unique<mamba::XonshActivator>();
        }
        else
        {
            std::cout << "Currently allowed values are: bash, zsh, cmd.exe & powershell" << std::endl;
            exit(1);
        }
        if (shell_options.action == "init")
        {
            init_shell(shell_options.shell_type, shell_options.prefix);
        }
        else if (shell_options.action == "hook")
        {
            Context::instance().root_prefix = shell_options.prefix;
            std::cout << activator->hook();
        }
        else if (shell_options.action == "activate")
        {
            if (shell_options.prefix == "base")
            {
                shell_options.prefix = Context::instance().root_prefix;
            }
            std::cout << activator->activate(shell_options.prefix, shell_options.stack);
        }
        else if (shell_options.action == "reactivate")
        {
            std::cout << activator->reactivate();
        }
        else if (shell_options.action == "deactivate")
        {
            std::cout << activator->deactivate();
        }
        else
        {
            std::cout << "Need an action (activate, deactivate or hook)";
            exit(1);
        }
    });
}

void install_specs(const std::vector<std::string>& specs)
{
    auto& ctx = Context::instance();
    set_global_options(ctx);

    Console::print(banner);

    if (ctx.target_prefix.empty())
    {
        std::cout << "No active prefix.\n\nRun $ micromamba activate <PATH_TO_MY_ENV>\nto activate an environment.\n";
        exit(1);
    }
    if (!fs::exists(ctx.target_prefix))
    {
        std::cout << "Prefix does not exist\n";
        exit(1);
    }

    fs::path cache_dir = ctx.root_prefix / "pkgs" / "cache";
    try
    {
        fs::create_directories(cache_dir);
    }
    catch (...)
    {
        std::cout << "Could not create `pkgs/cache/` dirs" << std::endl;
        exit(1);
    }

    std::vector<std::string> channel_urls = calculate_channel_urls(ctx.channels);

    std::vector<std::shared_ptr<MSubdirData>> subdirs;
    MultiDownloadTarget multi_dl;

    for (auto& url : channel_urls)
    {
        auto& channel = make_channel(url);
        std::string full_url = concat(channel.url(true), "/repodata.json");

        auto sdir = std::make_shared<MSubdirData>(
            concat(channel.name(), "/", channel.platform()),
            full_url,
            cache_dir / cache_fn_url(full_url)
        );

        sdir->load();
        multi_dl.add(sdir->target());
        subdirs.push_back(sdir);
    }
    multi_dl.download(true);

    std::vector<MRepo> repos;
    MPool pool;
    PrefixData prefix_data(ctx.target_prefix);
    prefix_data.load();
    auto repo = MRepo(pool, prefix_data);
    repos.push_back(repo);

    int prio_counter = subdirs.size();
    for (auto& subdir : subdirs)
    {
        MRepo repo = subdir->create_repo(pool);
        repo.set_priority(prio_counter--, 0);
        repos.push_back(repo);
    }

    MSolver solver(pool, {{SOLVER_FLAG_ALLOW_DOWNGRADE, 1}});
    solver.add_jobs(create_options.specs, SOLVER_INSTALL);
    solver.solve();

    mamba::MultiPackageCache package_caches({ctx.root_prefix / "pkgs"});
    mamba::MTransaction trans(solver, package_caches);

    if (ctx.json)
    {
        trans.log_json();
    }
    // TODO this is not so great
    std::vector<MRepo*> repo_ptrs;
    for (auto& r : repos) { repo_ptrs.push_back(&r); }

    std::cout << std::endl;
    bool yes = trans.prompt(ctx.root_prefix / "pkgs", repo_ptrs);
    if (!yes) exit(0);

    trans.execute(prefix_data, ctx.root_prefix / "pkgs");
}

void init_install_parser(CLI::App* subcom)
{
    subcom->add_option("specs", create_options.specs, "Specs to install into the new environment");
    init_network_parser(subcom);
    init_channel_parser(subcom);
    init_global_parser(subcom);

    subcom->callback([&]() {
        set_network_options(Context::instance());
        set_channels(Context::instance());
        install_specs(create_options.specs);
    });
}

void init_create_parser(CLI::App* subcom)
{
    subcom->add_option("specs", create_options.specs, "Specs to install into the new environment");
    subcom->add_option("-p,--prefix", create_options.prefix, "Path to the Prefix");
    init_network_parser(subcom);
    init_channel_parser(subcom);
    init_global_parser(subcom);
    // subcom->add_option("-n,--name" create_options.name, "Prefix name");

    subcom->callback([&]() {
        auto& ctx = Context::instance();
        ctx.target_prefix = create_options.prefix;

        set_network_options(ctx);
        set_channels(ctx);

        if (fs::exists(ctx.target_prefix))
        {
            std::cout << "Prefix already exists";
            exit(1);
        }
        else
        {
            fs::create_directories(ctx.target_prefix);
            fs::create_directories(ctx.target_prefix / "conda-meta");
            fs::create_directories(ctx.target_prefix / "pkgs");
        }
        install_specs(create_options.specs);

        return 0;
    });
}

int main(int argc, char** argv)
{
    CLI::App app{banner};

    CLI::App* shell_subcom = app.add_subcommand("shell", "Generate shell init scripts");
    init_shell_parser(shell_subcom);

    CLI::App* create_subcom = app.add_subcommand("create", "Create new environment");
    init_create_parser(create_subcom);

    CLI::App* install_subcom = app.add_subcommand("install", "Install packages in active environment");
    init_install_parser(install_subcom);

    // just for the help text
    app.footer(R"MRAW(To activate environments, use
    $ micromamba activate -p PATH/TO/PREFIX
to deactivate, use micromamba deactivate.
For this functionality to work, you need to initialize your shell with $ ./micromamba shell init
)MRAW");

    CLI11_PARSE(app, argc, argv);

    if (app.get_subcommands().size() == 0)
    {
        std::cout << app.help() << std::endl;
    }

    return 0;
}
