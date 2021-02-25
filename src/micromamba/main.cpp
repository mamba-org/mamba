// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <array>

#ifdef VENDORED_CLI11
#include "mamba/CLI.hpp"
#else
#include <CLI/CLI.hpp>
#endif

#include <yaml-cpp/yaml.h>

#include "mamba/activation.hpp"
#include "mamba/link.hpp"
#include "mamba/channel.hpp"
#include "mamba/context.hpp"
#include "mamba/config.hpp"
#include "mamba/output.hpp"
#include "mamba/prefix_data.hpp"
#include "mamba/pinning.hpp"
#include "mamba/repo.hpp"
#include "mamba/shell_init.hpp"
#include "mamba/solver.hpp"
#include "mamba/subdirdata.hpp"
#include "mamba/transaction.hpp"
#include "mamba/thread_utils.hpp"
#include "mamba/version.hpp"
#include "mamba/util.hpp"

#include "../thirdparty/termcolor.hpp"

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
    bool all;
    bool index_cache;
    bool packages;
    bool tarballs;
    // bool force_pkgs_dirs;
} clean_options;

static struct
{
    std::vector<std::string> specs;
    std::string prefix;
    std::string name;
    std::vector<std::string> files;
    std::vector<std::string> channels;
    bool override_channels = false;
    bool strict_channel_priority = false;
    std::string extra_safety_checks;
    bool no_pin = false;
} create_options;

static struct
{
    bool ssl_verify = true;
    std::size_t repodata_ttl = 1;
    bool retry_clean_cache = false;
    std::string cacert_path;
} network_options;

static struct
{
    bool show_sources = false;
} config_options;

static struct
{
    int verbosity = 0;
    bool always_yes = false;
    bool quiet = false;
    bool json = false;
    bool offline = false;
    bool dry_run = false;
    std::string rc_file = "";
    bool no_rc = false;
} global_options;

static struct
{
    std::string prefix;
    bool extract_conda_pkgs = false;
    bool extract_tarball = false;
} constructor_options;

struct formatted_pkg
{
    std::string name, version, build, channel;
};

bool
compare_alphabetically(const formatted_pkg& a, const formatted_pkg& b)
{
    return a.name < b.name;
}

void
check_root_prefix(bool silent = false)
{
    if (Context::instance().root_prefix.empty())
    {
        fs::path fallback_root_prefix = env::home_directory() / "micromamba";
        if (fs::exists(fallback_root_prefix) && !fs::is_empty(fallback_root_prefix))
        {
            if (!(fs::exists(fallback_root_prefix / "pkgs")
                  || fs::exists(fallback_root_prefix / "conda-meta")))
            {
                throw std::runtime_error(
                    mamba::concat("Could not use fallback root prefix ",
                                  fallback_root_prefix.string(),
                                  "\nDirectory exists, is not emtpy and not a conda prefix."));
            }
        }
        Context::instance().root_prefix = fallback_root_prefix;

        if (silent)
            return;

        // only print for the first time...
        if (!fs::exists(fallback_root_prefix))
        {
            std::cout << termcolor::yellow << "Warning: " << termcolor::reset
                      << "setting fallback $MAMBA_ROOT_PREFIX to " << fallback_root_prefix
                      << ".\n\n";
            std::cout
                << "You have not set a $MAMBA_ROOT_PREFIX environment variable.\nTo permanently modify the root prefix location, either set the "
                   "MAMBA_ROOT_PREFIX environment variable, or use\n  micromamba "
                   "shell init ... \nto initialize your shell, then restart or "
                   "source the contents of the shell init script.\n\n";
        }
        else
        {
            std::cout << termcolor::yellow << "Using fallback root prefix: " << termcolor::reset
                      << fallback_root_prefix << ".\n\n";
        }
    }
}

void
init_network_parser(CLI::App* subcom)
{
    subcom->add_option(
        "--ssl_verify", network_options.ssl_verify, "Enable or disable SSL verification");
    subcom->add_option("--cacert_path", network_options.cacert_path, "Path for CA Certificate");
    subcom->add_flag("--retry-with-clean-cache",
                     network_options.retry_clean_cache,
                     "If solve fails, try to fetch updated repodata.");
    subcom->add_option(
        "--repodata-ttl",
        network_options.repodata_ttl,
        "Repodata cache lifetime:\n 0 = always update\n 1 = respect HTTP header (default)\n>1 = cache lifetime in seconds");
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
    subcom->add_option("--rc-file", global_options.rc_file, "The unique configuration file to use");
    subcom->add_flag("--no-rc", global_options.no_rc, "Disable all configuration files");
}

void
set_network_options(Context& ctx)
{
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
        if (on_linux)
        {
            std::array<std::string, 6> cert_locations{
                "/etc/ssl/certs/ca-certificates.crt",                 // Debian/Ubuntu/Gentoo etc.
                "/etc/pki/tls/certs/ca-bundle.crt",                   // Fedora/RHEL 6
                "/etc/ssl/ca-bundle.pem",                             // OpenSUSE
                "/etc/pki/tls/cacert.pem",                            // OpenELEC
                "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",  // CentOS/RHEL 7
                "/etc/ssl/cert.pem",                                  // Alpine Linux
            };

            for (const auto& loc : cert_locations)
            {
                if (fs::exists(loc))
                {
                    ctx.ssl_verify = loc;
                }
            }
            if (ctx.ssl_verify.empty())
            {
                LOG_ERROR << "ssl_verify is enabled but no ca certificates found";
                exit(1);
            }
        }
    }

    ctx.local_repodata_ttl = network_options.repodata_ttl;
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
    check_root_prefix();

    if (!create_options.extra_safety_checks.empty())
    {
        if (to_lower(create_options.extra_safety_checks) == "none")
        {
            ctx.extra_safety_checks = VerificationLevel::NONE;
        }
        else if (to_lower(create_options.extra_safety_checks) == "warn")
        {
            ctx.extra_safety_checks = VerificationLevel::WARN;
        }
        else if (to_lower(create_options.extra_safety_checks) == "fail")
        {
            ctx.extra_safety_checks = VerificationLevel::FAIL;
        }
        else
        {
            LOG_ERROR << "Could not parse option for --extra-safety-checks\n"
                      << "Select none (default), warn or fail";
        }
    }
}

void
load_rc_files(Context& ctx)
{
    if (!global_options.no_rc)
    {
        ctx.load_config(global_options.rc_file);
    }
}

void
init_callback(Context& ctx)
{
    set_global_options(ctx);
    load_rc_files(ctx);
}

void
init_channel_parser(CLI::App* subcom)
{
    subcom->add_option("-c,--channel", create_options.channels)
        ->type_size(1)
        ->allow_extra_args(false);

    subcom->add_flag("--override-channels", create_options.override_channels, "Override channels");
    subcom->add_flag("--strict-channel-priority",
                     create_options.strict_channel_priority,
                     "Enable strict channel priority");
}

void
set_channels(Context& ctx)
{
    if (create_options.channels.empty())
    {
        char* comma_separated_channels = std::getenv("CONDA_CHANNELS");
        if (comma_separated_channels != nullptr)
        {
            std::stringstream channels_stream(comma_separated_channels);
            while (channels_stream.good())
            {
                std::string channel;
                std::getline(channels_stream, channel, ',');
                create_options.channels.push_back(channel);
            }
        }
    }

    if (create_options.override_channels && !ctx.override_channels_enabled)
    {
        LOG_WARNING << "override_channels is currently disabled by configuration (skipped)";
    }
    if (create_options.override_channels && ctx.override_channels_enabled)
    {
        ctx.channels = create_options.channels;
    }
    else
    {
        for (auto c : create_options.channels)
        {
            auto found_c = std::find(ctx.channels.begin(), ctx.channels.end(), c);
            if (found_c == ctx.channels.end())
            {
                ctx.channels.push_back(c);
            }
        }
    }
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
        check_root_prefix(true);

        if (shell_options.shell_type.empty())
        {
            // Doesnt work yet.
            // std::string guessed_shell = guess_shell();
            // if (!guessed_shell.empty())
            // {
            //     // std::cout << "Guessing shell " << termcolor::green << guessed_shell <<
            //     // termcolor::reset << std::endl;
            //     shell_options.shell_type = guessed_shell;
            // }
        }

        if (shell_options.shell_type == "bash" || shell_options.shell_type == "zsh"
            || shell_options.shell_type == "posix")
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
            throw std::runtime_error(
                "Currently allowed values are: posix, bash, xonsh, zsh, cmd.exe & powershell");
        }
        if (shell_options.action == "init")
        {
            if (shell_options.prefix == "base")
            {
                shell_options.prefix = Context::instance().root_prefix;
            }
            init_shell(shell_options.shell_type, shell_options.prefix);
        }
        else if (shell_options.action == "hook")
        {
            // TODO do we need to do something wtih `prefix -> root_prefix?`?
            std::cout << activator->hook();
        }
        else if (shell_options.action == "activate")
        {
            if (shell_options.prefix == "base" || shell_options.prefix.empty())
            {
                shell_options.prefix = Context::instance().root_prefix;
            }
            // checking wether we have a path or a name
            if (shell_options.prefix.find_first_of("/\\") == std::string::npos)
            {
                shell_options.prefix
                    = Context::instance().root_prefix / "envs" / shell_options.prefix;
            }
            if (!fs::exists(shell_options.prefix))
            {
                throw std::runtime_error(
                    "Cannot activate, environment does not exist: " + shell_options.prefix + "\n");
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
            throw std::runtime_error("Need an action (activate, deactivate or hook)");
        }
    });
}

MRepo
create_repo_from_pkgs_dir(MPool& pool, const fs::path& pkgs_dir)
{
    if (!fs::exists(pkgs_dir))
    {
        throw std::runtime_error("Specified pkgs_dir does not exist\n");
    }
    PrefixData prefix_data(pkgs_dir);
    prefix_data.load();
    for (const auto& entry : fs::directory_iterator(pkgs_dir))
    {
        fs::path info_json = entry.path() / "info" / "index.json";
        if (!fs::exists(info_json))
        {
            continue;
        }
        prefix_data.load_single_record(info_json);
    }
    return MRepo(pool, prefix_data);
}

int RETRY_SUBDIR_FETCH = 1 << 0;
int RETRY_SOLVE_ERROR = 1 << 1;

void
install_specs(const std::vector<std::string>& specs, bool create_env = false, int is_retry = 0)
{
    auto& ctx = Context::instance();
    set_global_options(ctx);

    if (!is_retry)
    {
        Console::print(banner);
    }

    fs::path pkgs_dirs;

    if (std::getenv("CONDA_PKGS_DIRS") != nullptr)
    {
        pkgs_dirs = fs::path(std::getenv("CONDA_PKGS_DIRS"));
    }
    else
    {
        pkgs_dirs = ctx.root_prefix / "pkgs";
    }

    if (ctx.target_prefix.empty())
    {
        throw std::runtime_error(
            "No active target prefix.\n\nRun $ micromamba activate <PATH_TO_MY_ENV>\nto activate an environment.\n");
    }
    if (!fs::exists(ctx.target_prefix) && create_env == false)
    {
        throw std::runtime_error("Prefix does not exist\n");
    }

    fs::path cache_dir = pkgs_dirs / "cache";
    try
    {
        fs::create_directories(cache_dir);
    }
    catch (...)
    {
        throw std::runtime_error("Could not create `pkgs/cache/` dirs");
    }

    std::vector<std::string> channel_urls = calculate_channel_urls(ctx.channels);

    std::vector<std::shared_ptr<MSubdirData>> subdirs;
    MultiDownloadTarget multi_dl;

    std::vector<std::pair<int, int>> priorities;
    int max_prio = static_cast<int>(channel_urls.size());
    std::string prev_channel_name;
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
        if (ctx.strict_channel_priority)
        {
            if (channel.name() != prev_channel_name)
            {
                max_prio--;
                prev_channel_name = channel.name();
            }
            priorities.push_back(std::make_pair(max_prio, channel.platform() == "noarch" ? 0 : 1));
        }
        else
        {
            priorities.push_back(std::make_pair(0, max_prio--));
        }
    }
    if (!ctx.offline)
    {
        multi_dl.download(true);
    }

    std::vector<MRepo> repos;
    MPool pool;
    if (ctx.offline)
    {
        LOG_INFO << "Creating repo from pkgs_dir for offline";
        repos.push_back(create_repo_from_pkgs_dir(pool, pkgs_dirs));
    }
    PrefixData prefix_data(ctx.target_prefix);
    prefix_data.load();

    auto repo = MRepo(pool, prefix_data);
    repos.push_back(repo);

    std::string prev_channel;
    bool loading_failed = false;
    for (std::size_t i = 0; i < subdirs.size(); ++i)
    {
        auto& subdir = subdirs[i];
        if (!subdir->loaded())
        {
            if (ctx.offline || mamba::ends_with(subdir->name(), "/noarch"))
            {
                continue;
            }
            else
            {
                throw std::runtime_error("Subdir " + subdir->name() + " not loaded!");
            }
        }

        auto& prio = priorities[i];
        try
        {
            MRepo repo = subdir->create_repo(pool);
            repo.set_priority(prio.first, prio.second);
            repos.push_back(repo);
        }
        catch (std::runtime_error& e)
        {
            if (is_retry & RETRY_SUBDIR_FETCH)
            {
                std::stringstream ss;
                ss << "Could not load repodata.json for " << subdir->name() << " after retry."
                   << "Please check repodata source. Exiting." << std::endl;
                throw std::runtime_error(ss.str());
            }

            std::cout << termcolor::yellow << "Could not load repodata.json for " << subdir->name()
                      << ". Deleting cache, and retrying." << termcolor::reset << std::endl;
            subdir->clear_cache();
            loading_failed = true;
        }
    }

    if (loading_failed)
    {
        if (!ctx.offline && !(is_retry & RETRY_SUBDIR_FETCH))
        {
            LOG_WARNING << "Encountered malformed repodata.json cache. Redownloading.";
            return install_specs(specs, create_env, is_retry | RETRY_SUBDIR_FETCH);
        }
        throw std::runtime_error("Could not load repodata. Cache corrupted?");
    }

    MSolver solver(pool, { { SOLVER_FLAG_ALLOW_DOWNGRADE, 1 } });
    solver.add_jobs(create_options.specs, SOLVER_INSTALL);

    if (!create_options.no_pin)
    {
        solver.add_pins(file_pins(prefix_data.path() / "conda-meta" / "pinned"));
        solver.add_pins(ctx.pinned_packages);
    }

    auto py_pin = python_pin(prefix_data, specs);
    if (!py_pin.empty())
    {
        solver.add_pin(py_pin);
    }

    bool success = solver.solve();
    if (!success)
    {
        std::cout << "\n" << solver.problems_to_str() << std::endl;
        if (network_options.retry_clean_cache && !(is_retry & RETRY_SOLVE_ERROR))
        {
            ctx.local_repodata_ttl = 2;
            return install_specs(specs, create_env, is_retry | RETRY_SOLVE_ERROR);
        }
        throw std::runtime_error("Could not solve for environment specs");
    }

    mamba::MultiPackageCache package_caches({ pkgs_dirs });
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
    bool yes = trans.prompt(pkgs_dirs, repo_ptrs);
    if (!yes)
        exit(0);

    if (create_env && !Context::instance().dry_run)
    {
        fs::create_directories(ctx.target_prefix / "conda-meta");
        fs::create_directories(ctx.target_prefix / "pkgs");
    }

    trans.execute(prefix_data, pkgs_dirs);
}

bool
remove_specs(const std::vector<std::string>& specs)
{
    auto& ctx = Context::instance();
    set_global_options(ctx);

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
    set_global_options(ctx);

    PrefixData prefix_data(ctx.target_prefix);
    prefix_data.load();

    if (ctx.json)
    {
        auto jout = nlohmann::json::array();
        std::vector<std::string> keys;

        for (const auto& pkg : prefix_data.m_package_records)
        {
            keys.push_back(pkg.first);
        }
        std::sort(keys.begin(), keys.end());

        for (const auto& key : keys)
        {
            auto obj = nlohmann::json();
            const auto& pkg_info = prefix_data.m_package_records.find(key)->second;
            auto channel = mamba::make_channel(pkg_info.url);
            obj["base_url"] = channel.base_url();
            obj["build_number"] = pkg_info.build_number;
            obj["build_string"] = pkg_info.build_string;
            obj["channel"] = channel.name();
            obj["dist_name"] = pkg_info.str();
            obj["name"] = pkg_info.name;
            obj["platform"] = pkg_info.subdir;
            obj["version"] = pkg_info.version;
            jout.push_back(obj);
        }
        std::cout << jout.dump(4) << std::endl;
        return;
    }

    std::cout << "List of packages in environment: " << ctx.target_prefix << std::endl;

    formatted_pkg formatted_pkgs;

    std::vector<formatted_pkg> packages;

    // order list of packages from prefix_data by alphabetical order
    for (const auto& package : prefix_data.m_package_records)
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

    std::sort(packages.begin(), packages.end(), compare_alphabetically);

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
init_list_parser(CLI::App* subcom)
{
    init_global_parser(subcom);

    subcom->callback([]() {
        auto& ctx = Context::instance();
        init_callback(ctx);
        list_packages();
    });
}

void
parse_file_options();

void
init_install_parser(CLI::App* subcom)
{
    subcom->add_option("specs", create_options.specs, "Specs to install into the environment");
    subcom->add_option("-p,--prefix", create_options.prefix, "Path to the Prefix");
    subcom->add_option("-n,--name", create_options.name, "Name of the Prefix");
    subcom->add_option("-f,--file", create_options.files, "File (yaml, explicit or plain)")
        ->type_size(1)
        ->allow_extra_args(false);
    subcom->add_flag(
        "--no-pin", create_options.no_pin, "Ignore pinned packages (from config or prefix file)");

    init_network_parser(subcom);
    init_channel_parser(subcom);
    init_global_parser(subcom);

    subcom->callback([&]() {
        auto& ctx = Context::instance();
        init_callback(ctx);
        set_network_options(ctx);
        ctx.strict_channel_priority = create_options.strict_channel_priority;

        if (!create_options.name.empty() && !create_options.prefix.empty())
        {
            throw std::runtime_error("Cannot set both, prefix and name.");
        }
        if (!create_options.name.empty())
        {
            if (create_options.name == "base")
            {
                ctx.target_prefix = ctx.root_prefix;
            }
            else
            {
                ctx.target_prefix = ctx.root_prefix / "envs" / create_options.name;
            }
        }
        else if (!create_options.prefix.empty())
        {
            ctx.target_prefix = create_options.prefix;
        }
        else if (ctx.target_prefix.empty())
        {
            throw std::runtime_error(
                "Prefix and name arguments are empty and a conda environment is not activated.");
        }

        ctx.target_prefix = fs::absolute(ctx.target_prefix);

        parse_file_options();
        set_channels(ctx);

        install_specs(create_options.specs, false);
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
init_config_parser(CLI::App* subcom)
{
    init_global_parser(subcom);

    auto& ctx = Context::instance();
    auto default_rc = (env::home_directory() / ".mambarc").string();
    auto root_rc = (ctx.root_prefix / ".mambarc").string();
    auto prefix_rc = (ctx.target_prefix / ".mambarc").string();

    /*
        auto config_loc_desc =
        unindent(R"(
            Config File Location Selection:
              Without one of these flags, the user config file at ')") + default_rc + R"(' is used.
            )";

        subcom->add_flag("--system")
              ->description("Write to the system .mambarc file at '" + root_rc + "'.")
              ->group(config_loc_desc);

        subcom->add_flag("--env")
              ->description("Write to the active conda environment .mambarc file (" + prefix_rc +
       "). " + "If no environment is active, write to the user config file (" + default_rc + ").")
              ->group(config_loc_desc);

        subcom->add_option("--file")
              ->description("Write to the given file.")
              ->group(config_loc_desc);
    */

    auto list_subcom = subcom->add_subcommand("list", "Show configuration values.");
    init_global_parser(list_subcom);
    list_subcom->add_flag("--show-source",
                          config_options.show_sources,
                          "Display all identified configuration sources.");

    list_subcom->callback([&]() {
        auto& ctx = Context::instance();
        init_callback(ctx);

        if (global_options.no_rc)
        {
            Console::print("Configuration files disabled by --no-rc flag");
        }
        else
        {
            Configurable config(global_options.rc_file);
            Console::print(config.dump(config_options.show_sources));
        }
        return 0;
    });

    auto sources_subcom = subcom->add_subcommand("sources", "Show configuration sources.");
    init_global_parser(sources_subcom);
    sources_subcom->callback([&]() {
        auto& ctx = Context::instance();
        init_callback(ctx);

        if (global_options.no_rc)
        {
            Console::print("Configuration files disabled by --no-rc flag");
        }
        else
        {
            Console::print("Configuration files (by precedence order):");

            Configurable config(global_options.rc_file);
            auto srcs = config.get_sources();
            auto valid_srcs = config.get_valid_sources();

            for (auto s : srcs)
            {
                auto found_s = std::find(valid_srcs.begin(), valid_srcs.end(), s);
                if (found_s != valid_srcs.end())
                {
                    Console::print(env::shrink_user(s).string());
                }
                else
                {
                    Console::print(env::shrink_user(s).string() + " (invalid)");
                }
            }
        }
        return 0;
    });
}

bool
download_explicit(const std::vector<PackageInfo>& pkgs)
{
    fs::path cache_path(Context::instance().root_prefix / "pkgs");
    // TODO better error handling for checking that cache path is
    // directory and writable etc.
    if (!fs::exists(cache_path))
    {
        fs::create_directories(cache_path);
    }
    std::vector<std::unique_ptr<PackageDownloadExtractTarget>> targets;
    MultiDownloadTarget multi_dl;
    MultiPackageCache pkg_cache({ Context::instance().root_prefix / "pkgs" });

    for (auto& pkg : pkgs)
    {
        targets.emplace_back(std::make_unique<PackageDownloadExtractTarget>(pkg));
        multi_dl.add(targets[targets.size() - 1]->target(cache_path, pkg_cache));
    }

    interruption_guard g([]() { Console::instance().init_multi_progress(); });

    bool downloaded = multi_dl.download(true);

    if (!downloaded)
    {
        LOG_ERROR << "Download didn't finish!";
        return false;
    }
    // make sure that all targets have finished extracting
    while (!is_sig_interrupted())
    {
        bool all_finished = true;
        for (const auto& t : targets)
        {
            if (!t->finished())
            {
                all_finished = false;
                break;
            }
        }
        if (all_finished)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return !is_sig_interrupted() && downloaded;
}

void
install_explicit_specs(std::vector<std::string>& specs)
{
    std::vector<mamba::MatchSpec> match_specs;
    std::vector<mamba::PackageInfo> pkg_infos;
    mamba::History hist(Context::instance().target_prefix);
    auto hist_entry = History::UserRequest::prefilled();
    std::string python_version;  // for pyc compilation
    // TODO unify this
    for (auto& spec : specs)
    {
        if (strip(spec).size() == 0)
            continue;
        std::size_t hash = spec.find_first_of('#');
        match_specs.emplace_back(spec.substr(0, hash));
        auto& ms = match_specs.back();
        PackageInfo p(ms.name);
        p.url = ms.url;
        p.build_string = ms.build;
        p.version = ms.version;
        p.channel = ms.channel;
        p.fn = ms.fn;

        if (hash != std::string::npos)
        {
            ms.brackets["md5"] = spec.substr(hash + 1);
            p.md5 = spec.substr(hash + 1);
        }
        hist_entry.update.push_back(ms.str());
        pkg_infos.push_back(p);

        if (ms.name == "python")
        {
            python_version = ms.version;
        }
    }
    if (download_explicit(pkg_infos))
    {
        // pkgs can now be linked
        fs::create_directories(Context::instance().target_prefix / "conda-meta");

        TransactionContext ctx(Context::instance().target_prefix, python_version);
        for (auto& pkg : pkg_infos)
        {
            LinkPackage lp(pkg, Context::instance().root_prefix / "pkgs", &ctx);
            std::cout << "Linking " << pkg.str() << "\n";
            hist_entry.link_dists.push_back(pkg.long_str());
            lp.execute();
        }

        hist.add_entry(hist_entry);
    }
}


void
set_target_prefix()
{
    auto& ctx = Context::instance();

    if (!create_options.name.empty() && !create_options.prefix.empty())
    {
        throw std::runtime_error("Cannot set both, prefix and name.");
    }
    if (!create_options.name.empty())
    {
        if (create_options.name == "base")
        {
            throw std::runtime_error(
                "Cannot create environment with name 'base'.");  // TODO! Use install -n.
        }

        ctx.target_prefix = ctx.root_prefix / "envs" / create_options.name;
    }
    else
    {
        if (create_options.prefix.empty())
        {
            throw std::runtime_error("Prefix and name arguments are empty.");
        }
        ctx.target_prefix = create_options.prefix;
    }

    ctx.target_prefix = fs::absolute(ctx.target_prefix);

    if (fs::exists(ctx.target_prefix))
    {
        if (fs::exists(ctx.target_prefix / "conda-meta"))
        {
            if (Console::prompt(
                    "Found conda-prefix in " + ctx.target_prefix.string() + ". Overwrite?", 'n'))
            {
                fs::remove_all(ctx.target_prefix);
            }
            else
            {
                exit(1);
            }
        }
        else
        {
            throw std::runtime_error("Non-conda folder exists at prefix. Exiting.");
        }
    }
}


void
parse_file_options()
{
    if (create_options.files.size() == 0)
    {
        return;
    }
    for (auto& file : create_options.files)
    {
        if ((ends_with(file, ".yml") || ends_with(file, ".yaml"))
            && create_options.files.size() != 1)
        {
            throw std::runtime_error("Can only handle 1 yaml file!");
        }
    }

    for (auto& file : create_options.files)
    {
        // read specs from file :)
        if (ends_with(file, ".yml") || ends_with(file, ".yaml"))
        {
            YAML::Node config = YAML::LoadFile(file);
            if (config["channels"])
            {
                std::vector<std::string> yaml_channels;
                try
                {
                    yaml_channels = config["channels"].as<std::vector<std::string>>();
                }
                catch (YAML::Exception& e)
                {
                    throw std::runtime_error(
                        mamba::concat("Could not read 'channels' as list of strings from ", file));
                }

                for (const auto& c : yaml_channels)
                {
                    create_options.channels.push_back(c);
                }
            }
            else
            {
                std::cout << termcolor::yellow << "WARNING: No channels specified in " << file
                          << termcolor::reset << std::endl;
            }
            if (create_options.name.empty() && create_options.prefix.empty())
            {
                try
                {
                    create_options.name = config["name"].as<std::string>();
                }
                catch (YAML::Exception& e)
                {
                    throw std::runtime_error(mamba::concat(
                        "Could not read environment 'name' as string from ",
                        file,
                        " and no name (-n) or prefix (-p) given on the command line"));
                }
            }
            try
            {
                create_options.specs = config["dependencies"].as<std::vector<std::string>>();
            }
            catch (YAML::Exception& e)
            {
                throw std::runtime_error(mamba::concat(
                    "Could not read environment 'dependencies' as list of strings from ", file));
            }
        }
        else
        {
            std::vector<std::string> file_contents = read_lines(file);
            if (file_contents.size() == 0)
            {
                throw std::runtime_error("file is empty");
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
                        for (std::size_t j = 0; j < i; ++j)
                        {
                            platform = file_contents[j];
                            if (starts_with(platform, "# platform: "))
                            {
                                platform = platform.substr(12);
                                break;
                            }
                        }
                    }

                    std::cout << "Installing explicit specs for platform " << platform << std::endl;
                    std::vector<std::string> explicit_specs(file_contents.begin() + i + 1,
                                                            file_contents.end());
                    set_target_prefix();
                    install_explicit_specs(explicit_specs);
                    exit(0);
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


void
init_create_parser(CLI::App* subcom)
{
    subcom->add_option("specs", create_options.specs, "Specs to install into the new environment");
    subcom->add_option("-p,--prefix", create_options.prefix, "Path to the Prefix");
    subcom->add_option("-n,--name", create_options.name, "Name of the Prefix");
    subcom->add_option("-f,--file", create_options.files, "File (yaml, explicit or plain)")
        ->type_size(1)
        ->allow_extra_args(false);
    subcom->add_flag(
        "--no-pin", create_options.no_pin, "Ignore pinned packages (from config or prefix file)");

    subcom->add_option(
        "--extra-safety-checks", create_options.extra_safety_checks, "Perform extra safety checks");

    init_network_parser(subcom);
    init_channel_parser(subcom);
    init_global_parser(subcom);

    subcom->callback([&]() {
        auto& ctx = Context::instance();
        init_callback(ctx);
        set_network_options(ctx);
        ctx.strict_channel_priority = create_options.strict_channel_priority;

        // file options have to be parsed _before_ the following checks
        // to fill in name and prefix
        parse_file_options();
        set_target_prefix();
        set_channels(ctx);

        install_specs(create_options.specs, true);

        return 0;
    });
}

void
init_clean_parser(CLI::App* subcom)
{
    init_global_parser(subcom);
    subcom->add_flag("-a,--all",
                     clean_options.all,
                     "Remove index cache, lock files, unused cache packages, and tarballs.");
    subcom->add_flag("-i,--index-cache", clean_options.index_cache, "Remove index cache.");
    subcom->add_flag("-p,--packages",
                     clean_options.packages,
                     "Remove unused packages from writable package caches.\n"
                     "WARNING: This does not check for packages installed using\n"
                     "symlinks back to the package cache.");
    subcom->add_flag("-t,--tarballs", clean_options.tarballs, "Remove cached package tarballs.");


    subcom->callback([&]() {
        auto& ctx = Context::instance();
        init_callback(ctx);
        Console::print("Cleaning up ... ");

        std::vector<fs::path> envs;

        MultiPackageCache caches(ctx.pkgs_dirs);
        if (!ctx.dry_run && (clean_options.index_cache || clean_options.all))
        {
            for (auto* pkg_cache : caches.writable_caches())
                if (fs::exists(pkg_cache->get_pkgs_dir() / "cache"))
                {
                    try
                    {
                        fs::remove_all(pkg_cache->get_pkgs_dir() / "cache");
                    }
                    catch (...)
                    {
                        LOG_WARNING << "Could not clean " << pkg_cache->get_pkgs_dir() / "cache";
                    }
                }
        }

        if (fs::exists(ctx.root_prefix / "conda-meta"))
        {
            envs.push_back(ctx.root_prefix);
        }

        for (auto& p : fs::directory_iterator(ctx.root_prefix / "envs"))
        {
            if (p.is_directory() && fs::exists(p.path() / "conda-meta"))
            {
                LOG_INFO << "Found environment: " << p.path();
                envs.push_back(p);
            }
        }

        // globally, collect installed packages
        std::set<std::string> installed_pkgs;
        for (auto& env : envs)
        {
            for (auto& pkg : fs::directory_iterator(env / "conda-meta"))
            {
                if (ends_with(pkg.path().string(), ".json"))
                {
                    std::string pkg_name = pkg.path().filename().string();
                    installed_pkgs.insert(pkg_name.substr(0, pkg_name.size() - 5));
                }
            }
        }

        auto get_file_size = [](const auto& s) -> std::string {
            std::stringstream ss;
            to_human_readable_filesize(ss, s);
            return ss.str();
        };

        auto collect_tarballs = [&]() {
            std::vector<fs::path> res;
            std::size_t total_size = 0;
            std::vector<printers::FormattedString> header = { "Package file", "Size" };
            mamba::printers::Table t(header);
            t.set_alignment({ printers::alignment::left, printers::alignment::right });
            t.set_padding({ 2, 4 });

            for (auto* pkg_cache : caches.writable_caches())
            {
                std::string header_line
                    = concat("Package cache folder: ", pkg_cache->get_pkgs_dir().string());
                std::vector<std::vector<printers::FormattedString>> rows;
                for (auto& p : fs::directory_iterator(pkg_cache->get_pkgs_dir()))
                {
                    std::string fname = p.path().filename();
                    if (!p.is_directory()
                        && (ends_with(p.path().string(), ".tar.bz2")
                            || ends_with(p.path().string(), ".conda")))
                    {
                        res.push_back(p.path());
                        rows.push_back(
                            { p.path().filename().string(), get_file_size(p.file_size()) });
                        total_size += p.file_size();
                    }
                }
                std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
                    return a[0].s < b[0].s;
                });
                t.add_rows(pkg_cache->get_pkgs_dir().string(), rows);
            }
            if (total_size)
            {
                t.add_rows({}, { { "Total size: ", get_file_size(total_size) } });
                t.print(std::cout);
            }
            return res;
        };

        if (clean_options.all || clean_options.tarballs)
        {
            auto to_be_removed = collect_tarballs();
            if (to_be_removed.size() == 0)
            {
                Console::print("No cached tarballs found");
            }
            else if (!ctx.dry_run && Console::prompt("\n\nRemove tarballs", 'y'))
            {
                for (auto& tbr : to_be_removed)
                {
                    fs::remove(tbr);
                }
                Console::print("\n\n");
            }
        }

        auto get_folder_size = [](auto& p) {
            std::size_t size = 0;
            for (auto& fp : fs::recursive_directory_iterator(p))
            {
                if (!fp.is_symlink())
                {
                    size += fp.file_size();
                }
            }
            return size;
        };

        auto collect_package_folders = [&]() {
            std::vector<fs::path> res;
            std::size_t total_size = 0;
            std::vector<printers::FormattedString> header = { "Package folder", "Size" };
            mamba::printers::Table t(header);
            t.set_alignment({ printers::alignment::left, printers::alignment::right });
            t.set_padding({ 2, 4 });

            for (auto* pkg_cache : caches.writable_caches())
            {
                std::string header_line
                    = concat("Package cache folder: ", pkg_cache->get_pkgs_dir().string());
                std::vector<std::vector<printers::FormattedString>> rows;
                for (auto& p : fs::directory_iterator(pkg_cache->get_pkgs_dir()))
                {
                    std::string fname = p.path().filename();
                    if (p.is_directory() && fs::exists(p.path() / "info" / "index.json"))
                    {
                        if (installed_pkgs.find(p.path().filename()) != installed_pkgs.end())
                        {
                            // do not remove installed packages
                            continue;
                        }
                        res.push_back(p.path());
                        std::size_t folder_size = get_folder_size(p);
                        rows.push_back(
                            { p.path().filename().string(), get_file_size(folder_size) });
                        total_size += folder_size;
                    }
                }
                std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
                    return a[0].s < b[0].s;
                });
                t.add_rows(pkg_cache->get_pkgs_dir().string(), rows);
            }
            if (total_size)
            {
                t.add_rows({}, { { "Total size: ", get_file_size(total_size) } });
                t.print(std::cout);
            }
            return res;
        };

        if (clean_options.all || clean_options.packages)
        {
            auto to_be_removed = collect_package_folders();
            if (to_be_removed.size() == 0)
            {
                Console::print("No cached tarballs found");
            }
            else if (!ctx.dry_run && Console::prompt("\n\nRemove unused packages", 'y'))
            {
                for (auto& tbr : to_be_removed)
                {
                    fs::remove_all(tbr);
                }
            }
        }
    });
}

void
read_binary_from_stdin_and_write_to_file(fs::path& filename)
{
    std::ofstream out_stream(filename.string().c_str(), std::ofstream::binary);
    // Need to reopen stdin as binary
    std::freopen(nullptr, "rb", stdin);
    if (std::ferror(stdin))
    {
        throw std::runtime_error("Re-opening stdin as binary failed.");
    }
    std::size_t len;
    std::array<char, 1024> buffer;

    while ((len = std::fread(buffer.data(), sizeof(char), buffer.size(), stdin)) > 0)
    {
        if (std::ferror(stdin) && !std::feof(stdin))
        {
            throw std::runtime_error("Reading from stdin failed.");
        }
        out_stream.write(buffer.data(), len);
    }
    out_stream.close();
}

void
init_constructor_parser(CLI::App* subcom)
{
    subcom->add_option("-p,--prefix", constructor_options.prefix, "Path to the Prefix");
    subcom->add_flag("--extract-conda-pkgs",
                     constructor_options.extract_conda_pkgs,
                     "Extract the conda pkgs in <prefix>/pkgs");
    subcom->add_flag("--extract-tarball",
                     constructor_options.extract_tarball,
                     "Extract given tarball into prefix");

    subcom->callback([&]() {
        if (constructor_options.prefix.empty())
        {
            throw std::runtime_error("Prefix is required.");
        }
        if (constructor_options.extract_conda_pkgs)
        {
            fs::path pkgs_dir = constructor_options.prefix;
            fs::path filename;
            pkgs_dir = pkgs_dir / "pkgs";
            for (const auto& entry : fs::directory_iterator(pkgs_dir))
            {
                filename = entry.path().filename();
                if (ends_with(filename.string(), ".tar.bz2")
                    || ends_with(filename.string(), ".conda"))
                {
                    extract(entry.path());
                }
            }
        }
        if (constructor_options.extract_tarball)
        {
            fs::path extract_tarball_path = fs::path(constructor_options.prefix) / "_tmp.tar.bz2";
            read_binary_from_stdin_and_write_to_file(extract_tarball_path);
            extract_archive(extract_tarball_path, constructor_options.prefix);
            fs::remove(extract_tarball_path);
        }
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

    // initial stuff
    Context::instance().is_micromamba = true;

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
    init_list_parser(list_subcom);

    CLI::App* config_subcom = app.add_subcommand("config", "Configuration of micromamba");
    init_config_parser(config_subcom);

    std::stringstream footer;  // just for the help text

    footer
        << "In order to use activate and deactivate you need to initialize your shell.\n"
        << "Either call shell init ... or execute the shell hook directly:\n\n"
        << "    $ eval \"$(./micromamba shell hook -s bash)\"\n\n"
        << "and then run activate and deactivate like so:\n\n"
        << "    $ micromamba activate  " << termcolor::white
        << "# notice the missing dot in front of the command\n\n"
        << "To permanently initialize your shell, use shell init like so:\n\n"
        << "    $ ./micromamba shell init -s bash -p /home/$USER/micromamba \n\n"
        << "and restart your shell. Supported shells are bash, zsh, xonsh, cmd.exe, and powershell."
        << termcolor::reset;

    app.footer(footer.str());

    CLI::App* constructor_subcom
        = app.add_subcommand("constructor", "Commands to support using micromamba in constructor");
    init_constructor_parser(constructor_subcom);

    try
    {
        CLI11_PARSE(app, argc, argv);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << e.what();
        set_sig_interrupted();
        return 1;
    }

    if (app.get_subcommands().size() == 0)
    {
        std::cout << app.help() << std::endl;
    }

    if (app.got_subcommand(config_subcom) && config_subcom->get_subcommands().size() == 0)
    {
        std::cout << config_subcom->help() << std::endl;
    }

    return 0;
}
