// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>

#ifdef VENDORED_CLI11
    #include "mamba/CLI.hpp"
#else
    #include <CLI/CLI.hpp>
#endif

#include <yaml-cpp/yaml.h>

#include "mamba/activation.hpp"
#include "mamba/channel.hpp"
#include "mamba/context.hpp"
#include "mamba/output.hpp"
#include "mamba/prefix_data.hpp"
#include "mamba/repo.hpp"
#include "mamba/shell_init.hpp"
#include "mamba/solver.hpp"
#include "mamba/subdirdata.hpp"
#include "mamba/transaction.hpp"
#include "mamba/version.hpp"
#include "mamba/util.hpp"

const char banner[] = R"MAMBARAW(
                                           __
          __  ______ ___  ____ _____ ___  / /_  ____ _
         / / / / __ `__ \/ __ `/ __ `__ \/ __ \/ __ `/
        / /_/ / / / / / / /_/ / / / / / / /_/ / /_/ /
       / .___/_/ /_/ /_/\__,_/_/ /_/ /_/_.___/\__,_/
      /_/
)MAMBARAW";

using namespace mamba;  // NOLINT(build/namespaces)

static struct
{
    bool hook;
    std::string shell_type;
    std::string action;
    std::string prefix = "base";
    bool stack;
} shell_options;

static struct
{
    std::vector<std::string> specs;
    std::string prefix;
    std::string name;
    std::vector<std::string> files;
    std::vector<std::string> channels;
    bool override_channels = false;  // currently a no-op!
} create_options;

static struct
{
    bool ssl_verify = true;
    std::string cacert_path;
} network_options;

static struct
{
    int verbosity = 0;
    bool always_yes = false;
    bool quiet = false;
    bool json = false;
    bool offline = false;
    bool dry_run = false;
} global_options;

struct formatted_pkg
{
    std::string name, version, build, channel;
};

bool
compareAlphabetically(const formatted_pkg& a, const formatted_pkg& b)
{
    return a.name < b.name;
}

void
init_network_parser(CLI::App* subcom)
{
    subcom->add_option(
        "--ssl_verify", network_options.ssl_verify, "Enable or disable SSL verification");
    subcom->add_option("--cacert_path", network_options.cacert_path, "Path for CA Certificate");
}

void
init_global_parser(CLI::App* subcom)
{
    subcom->add_flag("-v,--verbose",
                     global_options.verbosity,
                     "Enbable verbose mode (higher verbosity with multiple -v, e.g. -vvv)");
    subcom->add_flag("-q,--quiet", global_options.quiet, "Quiet mode (print less output)");
    subcom->add_flag(
        "-y,--yes", global_options.always_yes, "Automatically answer yes on all questions");
    subcom->add_flag("--json", global_options.json, "Report all output as json");
    subcom->add_flag("--offline", global_options.offline, "Force use cached repodata");
    subcom->add_flag("--dry-run", global_options.dry_run, "Only display what would have been done");
}

void
set_network_options(Context& ctx)
{
    std::array<std::string, 6> cert_locations{
        "/etc/ssl/certs/ca-certificates.crt",                 // Debian/Ubuntu/Gentoo etc.
        "/etc/pki/tls/certs/ca-bundle.crt",                   // Fedora/RHEL 6
        "/etc/ssl/ca-bundle.pem",                             // OpenSUSE
        "/etc/pki/tls/cacert.pem",                            // OpenELEC
        "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",  // CentOS/RHEL 7
        "/etc/ssl/cert.pem",                                  // Alpine Linux
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

void
set_global_options(Context& ctx)
{
    ctx.set_verbosity(global_options.verbosity);
    ctx.quiet = global_options.quiet;
    ctx.json = global_options.json;
    ctx.always_yes = global_options.always_yes;
    ctx.offline = global_options.offline;
    ctx.dry_run = global_options.dry_run;
}

void
init_channel_parser(CLI::App* subcom)
{
    subcom->add_option("-c,--channel", create_options.channels)
          ->type_size(1)
          ->allow_extra_args(false)
    ;

    subcom->add_flag("--override-channels",
                     create_options.override_channels,
                     "Override channels (ignored, because micromamba has no default channels)");
}

void
set_channels(Context& ctx)
{
    ctx.channels = create_options.channels;
}

void
init_shell_parser(CLI::App* subcom)
{
    subcom->add_option("-s,--shell",
                       shell_options.shell_type,
                       "A shell type (bash, fish, posix, powershell, xonsh)");
    subcom->add_option("--stack",
                       shell_options.stack,
                       "Stack the environment being activated on top of the "
                       "previous active environment, "
                       "rather replacing the current active environment with a "
                       "new one. Currently, "
                       "only the PATH environment variable is stacked. "
                       "This may be enabled implicitly by the 'auto_stack' "
                       "configuration variable.");

    subcom->add_option("action", shell_options.action, "activate, deactivate or hook");
    // TODO add custom validator here!
    subcom->add_option("-p,--prefix",
                       shell_options.prefix,
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
            std::cout << "Currently allowed values are: bash, zsh, cmd.exe & powershell"
                      << std::endl;
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
            // checking wether we have a path or a name
            if (shell_options.prefix.find_first_of("/\\") == std::string::npos)
            {
                if (Context::instance().root_prefix.empty())
                {
                    throw std::runtime_error(
                        "Trying to activate environment by name, but MAMBA_ROOT_PREFIX environment variable is not set.\nPlease run micromamba shell init.");
                }
                shell_options.prefix
                    = Context::instance().root_prefix / "envs" / shell_options.prefix;
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

void
install_specs(const std::vector<std::string>& specs, bool create_env = false)
{
    auto& ctx = Context::instance();

    set_global_options(ctx);

    Console::print(banner);

    if (ctx.root_prefix.empty())
    {
        std::cout << "You have not set a $MAMBA_ROOT_PREFIX.\nEither set the "
                     "MAMBA_ROOT_PREFIX environment variable, or use\n  micromamba "
                     "shell init ... \nto initialize your shell, then restart or "
                     "source the contents of the shell init script.\n";
        exit(1);
    }

    if (ctx.target_prefix.empty())
    {
        std::cout << "No active target prefix.\n\nRun $ micromamba activate "
                     "<PATH_TO_MY_ENV>\nto activate an environment.\n";
        exit(1);
    }
    if (!fs::exists(ctx.target_prefix) && create_env == false)
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

        auto sdir = std::make_shared<MSubdirData>(concat(channel.name(), "/", channel.platform()),
                                                  full_url,
                                                  cache_dir / cache_fn_url(full_url));

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

    MSolver solver(pool, { { SOLVER_FLAG_ALLOW_DOWNGRADE, 1 } });
    solver.add_jobs(create_options.specs, SOLVER_INSTALL);
    bool success = solver.solve();
    if (!success)
    {
        std::cout << "\n" << solver.problems_to_str() << std::endl;
        exit(1);
    }

    mamba::MultiPackageCache package_caches({ ctx.root_prefix / "pkgs" });
    mamba::MTransaction trans(solver, package_caches);

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

    if (create_env && !Context::instance().dry_run)
    {
        fs::create_directories(ctx.target_prefix);
        fs::create_directories(ctx.target_prefix / "conda-meta");
        fs::create_directories(ctx.target_prefix / "pkgs");
    }

    trans.execute(prefix_data, ctx.root_prefix / "pkgs");
}

bool
remove_specs(const std::vector<std::string>& specs)
{
    auto& ctx = Context::instance();

    set_global_options(ctx);

    Console::print(banner);

    if (ctx.root_prefix.empty())
    {
        std::cout << "You have not set a $MAMBA_ROOT_PREFIX.\nEither set the "
                     "MAMBA_ROOT_PREFIX environment variable, or use\n  micromamba "
                     "shell init ... \nto initialize your shell, then restart or "
                     "source the contents of the shell init script.\n";
        exit(1);
    }

    if (ctx.target_prefix.empty())
    {
        std::cout << "No active target prefix.\n\nRun $ micromamba activate "
                     "<PATH_TO_MY_ENV>\nto activate an environment.\n";
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
    solver.add_jobs(create_options.specs, SOLVER_ERASE);
    solver.solve();

    mamba::MultiPackageCache package_caches({ ctx.root_prefix / "pkgs" });
    mamba::MTransaction trans(solver, package_caches);

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

    return true;
}

void
list_packages()
{
    auto& ctx = Context::instance();
    PrefixData prefix_data(ctx.target_prefix);
    prefix_data.load();

    std::cout << "List of packages in environment: " << ctx.target_prefix << std::endl;

    formatted_pkg formatted_pkgs;

    std::vector<formatted_pkg> packages;

    // order list of packages from prefix_data by alphabetical order
    for (auto package : prefix_data.m_package_records)
    {
        formatted_pkgs.name = package.second.name;
        formatted_pkgs.version = package.second.version;
        formatted_pkgs.build = package.second.build_string;
        if (package.second.channel.find("https://repo.anaconda.com/pkgs/") == 0)
        {
            formatted_pkgs.channel = "";
        }
        else
        {
            Channel& channel = make_channel(package.second.url);
            formatted_pkgs.channel = channel.name();
        }
        packages.push_back(formatted_pkgs);
    }

    std::sort(packages.begin(), packages.end(), compareAlphabetically);

    // format and print table
    printers::Table t({ "Name", "Version", "Build", "Channel" });
    t.set_alignment({ printers::alignment::left,
                      printers::alignment::left,
                      printers::alignment::left,
                      printers::alignment::left });
    t.set_padding({ 2, 2, 2, 2 });

    for (auto p : packages)
    {
        t.add_row({ p.name, p.version, p.build, p.channel });
    }

    t.print(std::cout);
}

void
init_install_parser(CLI::App* subcom)
{
    subcom->add_option("specs", create_options.specs, "Specs to install into the environment");
    init_network_parser(subcom);
    init_channel_parser(subcom);
    init_global_parser(subcom);

    subcom->callback([&]() {
        set_network_options(Context::instance());
        set_channels(Context::instance());
        install_specs(create_options.specs);
    });
}

void
init_remove_parser(CLI::App* subcom)
{
    subcom->add_option("specs", create_options.specs, "Specs to remove from the environment");
    init_global_parser(subcom);

    subcom->callback([&]() { remove_specs(create_options.specs); });
}

void
install_explicit_specs(std::vector<std::string>& specs)
{
    std::vector<mamba::MatchSpec> match_specs;

    for (auto& spec : specs)
    {
        std::size_t hash = spec.find_first_of('#');
        std::cout << "adding spec " << spec.substr(0, hash) << std::endl;
        match_specs.emplace_back(spec.substr(0, hash));
        if (hash != std::string::npos)
        {
            match_specs.back().brackets["md5"] = spec.substr(hash + 1);
        }
    }

    for (auto& ms : match_specs)
    {
        std::cout << ms.str() << std::endl;
    }
}

void
init_create_parser(CLI::App* subcom)
{
    subcom->add_option("specs", create_options.specs, "Specs to install into the new environment");
    subcom->add_option("-p,--prefix", create_options.prefix, "Path to the Prefix");
    subcom->add_option("-n,--name", create_options.name, "Name of the Prefix");
    subcom->add_option("-f,--file", create_options.files, "File (yaml, explicit or plain)")
          ->type_size(1)
          ->allow_extra_args(false)
    ;
    init_network_parser(subcom);
    init_channel_parser(subcom);
    init_global_parser(subcom);

    subcom->callback([&]() {
        auto& ctx = Context::instance();
        set_global_options(ctx);

        if (create_options.files.size() != 0)
        {
            for (auto& file : create_options.files)
            {
                if ((ends_with(file, ".yml") || ends_with(file, ".yaml")) && create_options.files.size() != 1)
                {
                    std::cout << "Can only handle 1 yaml file!" << std::endl;
                    exit(1);
                }
            }

            for (auto& file : create_options.files)
            {
                // read specs from file :)
                if (ends_with(file, ".yml") || ends_with(file, ".yaml"))
                {
                    YAML::Node config = YAML::LoadFile(file);
                    create_options.channels = config["channels"].as<std::vector<std::string>>();
                    create_options.name = config["name"].as<std::string>();
                    create_options.specs = config["dependencies"].as<std::vector<std::string>>();
                }
                else
                {
                    std::vector<std::string> file_contents = read_lines(file);
                    if (file_contents.size() == 0)
                    {
                        std::cout << "file is empty" << std::endl;
                        exit(1);
                    }
                    for (std::size_t i = 0; i < file_contents.size(); ++i)
                    {
                        auto& line = file_contents[i];
                        if (starts_with(line, "@EXPLICIT"))
                        {
                            // this is an explicit env
                            // we can check if the platform is correct with the previous line
                            std::string platform;
                            if (i >= 1)
                            {
                                platform = file_contents[i - 1];
                                if (starts_with(platform, "# platform: "))
                                {
                                    platform = platform.substr(12);
                                }
                            }

                            std::cout << "Installing explicit specs for platform " << platform
                                      << std::endl;
                            std::cout << "Explicit spec installation is a work-in-progress. Exiting."
                                      << std::endl;
                            exit(1);
                            std::vector<std::string> explicit_specs(file_contents.begin() + i + 1,
                                                                    file_contents.end());
                            install_explicit_specs(explicit_specs);
                        }
                    }

                    for (auto& line : file_contents)
                    {
                        if (line[0] == '#' || line[0] == '@')
                        {
                            // skip
                        }
                        else
                        {
                            create_options.specs.push_back(line);
                        }
                    }
                }
            }
        }

        if (!create_options.name.empty() && !create_options.prefix.empty())
        {
            throw std::runtime_error("Cannot set both, prefix and name.");
        }
        if (!create_options.name.empty())
        {
            ctx.target_prefix = Context::instance().root_prefix / "envs" / create_options.name;
        }
        else
        {
            if (create_options.prefix.empty())
            {
                throw std::runtime_error("Prefix and name arguments are empty.");
            }
            ctx.target_prefix = create_options.prefix;
        }

        set_network_options(ctx);
        set_channels(ctx);

        // if (fs::exists(ctx.target_prefix))
        // {
        //     std::cout << "Prefix already exists";
        //     exit(1);
        // }

        for (auto& s : create_options.specs)
        {
            std::cout << "looking for " << s << std::endl;
        }
        install_specs(create_options.specs, true);

        return 0;
    });
}

std::string
version()
{
    return mamba_version;
}

int
main(int argc, char** argv)
{
    CLI::App app{ std::string(banner) + "\nVersion: " + version() + "\n" };

    auto print_version = [](int count) {
        std::cout << version() << std::endl;
        exit(0);
    };
    app.add_flag_function("--version", print_version);

    CLI::App* shell_subcom = app.add_subcommand("shell", "Generate shell init scripts");
    init_shell_parser(shell_subcom);

    CLI::App* create_subcom = app.add_subcommand("create", "Create new environment");
    init_create_parser(create_subcom);

    CLI::App* install_subcom
        = app.add_subcommand("install", "Install packages in active environment");
    init_install_parser(install_subcom);

    CLI::App* remove_subcom
        = app.add_subcommand("remove", "Remove packages from active environment");
    init_remove_parser(remove_subcom);

    CLI::App* list_subcom = app.add_subcommand("list", "List packages in active environment");
    list_subcom->callback([]() { list_packages(); });
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
