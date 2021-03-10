// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "parsers.hpp"
#include "options.hpp"

#include "mamba/environment.hpp"
#include "mamba/fetch.hpp"
#include "mamba/config.hpp"

#include "../thirdparty/termcolor.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_rc_options(CLI::App* subcom)
{
    std::string config = "Configuration files options";
    subcom->add_option("--rc-file", general_options.rc_file, "The unique configuration file to use")
        ->group(config);
    subcom->add_flag("--no-rc", general_options.no_rc, "Disable all configuration files")
        ->group(config);
}

void
load_rc_options(Context& ctx)
{
    if (!general_options.no_rc)
    {
        Configurable::instance().load(general_options.rc_file);
        ctx.load_config();
    }
}

void
init_general_options(CLI::App* subcom)
{
    init_rc_options(subcom);

    std::string global = "Global options";
    subcom
        ->add_flag("-v,--verbose",
                   general_options.verbosity,
                   "Enable verbose mode (higher verbosity with multiple -v, e.g. -vvv)")
        ->group(global);
    subcom->add_flag("-q,--quiet", general_options.quiet, "Quiet mode (print less output)")
        ->group(global);
    subcom
        ->add_flag(
            "-y,--yes", general_options.always_yes, "Automatically answer yes on all questions")
        ->group(global);
    subcom->add_flag("--json", general_options.json, "Report all output as json")->group(global);
    subcom->add_flag("--offline", general_options.offline, "Force use cached repodata")
        ->group(global);
    subcom->add_flag("--dry-run", general_options.dry_run, "Only display what would have been done")
        ->group(global);
}

void
load_general_options(Context& ctx)
{
    ctx.set_verbosity(general_options.verbosity);
    ctx.quiet = general_options.quiet;
    ctx.json = general_options.json;
    ctx.always_yes = general_options.always_yes;
    ctx.offline = general_options.offline;
    ctx.dry_run = general_options.dry_run;
}

void
init_prefix_options(CLI::App* subcom)
{
    std::string prefix = "Prefix options";
    subcom->add_option("-r,--root-prefix", create_options.root_prefix, "Path to the root prefix")
        ->group(prefix);
    subcom->add_option("-p,--prefix", create_options.prefix, "Path to the target prefix")
        ->group(prefix);
    subcom->add_option("-n,--name", create_options.name, "Name of the prefix")->group(prefix);
}

void
load_prefix_options(Context& ctx)
{
    if (!create_options.name.empty() && !create_options.prefix.empty())
    {
        throw std::runtime_error("Cannot set both, prefix and name.");
    }

    if (!create_options.root_prefix.empty())
    {
        ctx.root_prefix = create_options.root_prefix;
    }
    check_root_prefix();

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

    if (ctx.target_prefix.empty())
    {
        throw std::runtime_error("Prefix and name arguments are empty.");
    }

    ctx.target_prefix = fs::absolute(ctx.target_prefix);
}

void
catch_existing_target_prefix(Context& ctx)
{
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
init_network_parser(CLI::App* subcom)
{
    std::string network = "Network options";
    subcom
        ->add_option(
            "--ssl-verify", network_options.ssl_verify, "Enable or disable SSL verification")
        ->group(network);
    subcom
        ->add_option("--ssl-no-revoke",
                     network_options.ssl_no_revoke,
                     "Enable or disable SSL certificate revocation checks (default: false)")
        ->group(network);
    subcom->add_option("--cacert-path", network_options.cacert_path, "Path for CA Certificate")
        ->group(network);
    subcom
        ->add_flag("--retry-with-clean-cache",
                   network_options.retry_clean_cache,
                   "If solve fails, try to fetch updated repodata.")
        ->group(network);
    subcom
        ->add_option(
            "--repodata-ttl",
            network_options.repodata_ttl,
            "Repodata cache lifetime:\n 0 = always update\n 1 = respect HTTP header (default)\n>1 = cache lifetime in seconds")
        ->group(network);
}

void
load_network_options(Context& ctx)
{
    // ssl verify can be either an empty string (regular SSL verification),
    // the string "<false>" to indicate no SSL verification, or a path to
    // a directory with cert files, or a cert file.
    if (network_options.ssl_verify == false || ctx.offline)
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
        else
        {
            ctx.ssl_verify = "<system>";
        }
    }

    ctx.ssl_no_revoke = network_options.ssl_no_revoke;
    ctx.local_repodata_ttl = network_options.repodata_ttl;

    init_curl_ssl();
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
load_channel_options(Context& ctx)
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

    SET_BOOLEAN_FLAG(strict_channel_priority);
}


void
check_root_prefix(bool silent)
{
    auto& ctx = Context::instance();
    if (ctx.root_prefix.empty() && env::get("CONDA_PKGS_DIRS").empty())
    {
        fs::path default_root_prefix;
        if (env::get("MAMBA_DEFAULT_ROOT_PREFIX").empty())
        {
            default_root_prefix = env::home_directory() / "micromamba";
        }
        else
        {
            default_root_prefix = env::get("MAMBA_DEFAULT_ROOT_PREFIX");
            LOG_WARNING << unindent(R"(
                            'MAMBA_DEFAULT_ROOT_PREFIX' is meant for testing purpose.
                            Consider using 'MAMBA_ROOT_PREFIX' instead)");
        }

        if (fs::exists(default_root_prefix) && !fs::is_empty(default_root_prefix))
        {
            if (!(fs::exists(default_root_prefix / "pkgs")
                  || fs::exists(default_root_prefix / "conda-meta")))
            {
                throw std::runtime_error(
                    mamba::concat("Could not use default root prefix ",
                                  default_root_prefix.string(),
                                  "\nDirectory exists, is not emtpy and not a conda prefix."));
            }
        }
        Context::instance().root_prefix = default_root_prefix;

        if (silent)
            return;

        LOG_WARNING << "Using default root prefix: " << default_root_prefix;
        if (!fs::exists(default_root_prefix))
        {
            LOG_WARNING << unindent(R"(
                            You have not set a $MAMBA_ROOT_PREFIX environment variable.
                            To permanently modify the root prefix location, either set the
                            MAMBA_ROOT_PREFIX environment variable, or use micromamba
                            shell init ... to initialize your shell, then restart or
                            source the contents of the shell init script.)");
        }
    }
}
