// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"
#include "info.hpp"

#include "mamba/environment.hpp"
#include "mamba/fetch.hpp"
#include "mamba/configuration.hpp"

#include "../thirdparty/termcolor.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_rc_options(CLI::App* subcom)
{
    auto& config = Configuration::instance();
    std::string cli_group = "Configuration options";

    auto& rc_file = config.at("rc_file").get_wrapped<std::string>();
    subcom->add_option("--rc-file", rc_file.set_cli_config(""), rc_file.description())
        ->group(cli_group);

    auto& no_rc = config.at("no_rc").get_wrapped<bool>();
    subcom->add_flag("--no-rc", no_rc.set_cli_config(0), no_rc.description())->group(cli_group);

    auto& no_env = config.at("no_env").get_wrapped<bool>();
    subcom->add_flag("--no-env", no_env.set_cli_config(0), no_env.description())->group(cli_group);
}

void
init_general_options(CLI::App* subcom)
{
    init_rc_options(subcom);

    auto& config = Configuration::instance();
    std::string cli_group = "Global options";

    auto& log_lvl = config.at("log_level").get_wrapped<LogLevel>();
    subcom
        ->add_set("--log-level",
                  log_lvl.set_cli_config(""),
                  { "off", "fatal", "error", "warning", "info", "debug", "trace" },
                  log_lvl.description())
        ->group(cli_group);

    auto& verbose = config.at("verbose").get_wrapped<std::uint8_t>();
    subcom->add_flag("-v,--verbose", verbose.set_cli_config(-1), verbose.description())
        ->group(cli_group);

    auto& quiet = config.at("quiet").get_wrapped<bool>();
    subcom->add_flag("-q,--quiet", quiet.set_cli_config(false), quiet.description())
        ->group(cli_group);

    auto& always_yes = config.at("always_yes").get_wrapped<bool>();
    subcom->add_flag("-y,--yes", always_yes.set_cli_config(false), always_yes.description())
        ->group(cli_group);

    auto& json = config.at("json").get_wrapped<bool>();
    subcom->add_flag("--json", json.set_cli_config(0), json.description())->group(cli_group);

    auto& offline = config.at("offline").get_wrapped<bool>();
    subcom->add_flag("--offline", offline.set_cli_config(false), offline.description())
        ->group(cli_group);

    auto& dry_run = config.at("dry_run").get_wrapped<bool>();
    subcom->add_flag("--dry-run", dry_run.set_cli_config(false), dry_run.description())
        ->group(cli_group);
}

void
init_prefix_options(CLI::App* subcom)
{
    auto& config = Configuration::instance();
    std::string cli_group = "Prefix options";

    auto& root = config.at("root_prefix").get_wrapped<fs::path>();
    subcom->add_option("-r,--root-prefix", root.set_cli_config(""), root.description())
        ->group(cli_group);

    auto& target = config.at("target_prefix").get_wrapped<fs::path>();
    subcom->add_option("-p,--prefix", target.set_cli_config(""), target.description())
        ->group(cli_group);

    auto& name = config.at("env_name").get_wrapped<std::string>();
    subcom->add_option("-n,--name", name.set_cli_config(""), name.description())->group(cli_group);
}

void
init_network_parser(CLI::App* subcom)
{
    auto& config = Configuration::instance();
    std::string cli_group = "Network options";

    auto& ssl_verify = config.at("ssl_verify").get_wrapped<std::string>();
    subcom->add_option("--ssl-verify", ssl_verify.set_cli_config(""), ssl_verify.description())
        ->group(cli_group);

    auto& ssl_no_revoke = config.at("ssl_no_revoke").get_wrapped<bool>();
    subcom
        ->add_option(
            "--ssl-no-revoke", ssl_no_revoke.set_cli_config(0), ssl_no_revoke.description())
        ->group(cli_group);

    auto& cacert_path = config.at("cacert_path").get_wrapped<std::string>();
    subcom->add_option("--cacert-path", cacert_path.set_cli_config(""), cacert_path.description())
        ->group(cli_group);

    auto& local_repodata_ttl = config.at("local_repodata_ttl").get_wrapped<std::size_t>();
    subcom
        ->add_option("--repodata-ttl",
                     local_repodata_ttl.set_cli_config(-1),
                     local_repodata_ttl.description())
        ->group(cli_group);

    auto& retry_clean_cache = config.at("retry_clean_cache").get_wrapped<bool>();
    subcom
        ->add_flag("--retry-clean-cache",
                   retry_clean_cache.set_cli_config(false),
                   retry_clean_cache.description())
        ->group(cli_group);
}


void
init_channel_parser(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    auto& channels = config.at("channels").get_wrapped<std::vector<std::string>>();
    channels.set_post_build_hook(channels_hook);
    subcom->add_option("-c,--channel", channels.set_cli_config({}), channels.description())
        ->type_size(1)
        ->allow_extra_args(false);

    auto& override_channels = config.insert(Configurable("override_channels", false)
                                                .group("cli")
                                                .rc_configurable(false)
                                                .set_env_var_name()
                                                .description("Override channels")
                                                .set_post_build_hook(override_channels_hook));
    subcom->add_flag("--override-channels",
                     override_channels.set_cli_config(false),
                     override_channels.description());

    auto& channel_priority = config.at("channel_priority").get_wrapped<ChannelPriority>();
    subcom->add_set("--channel-priority",
                    channel_priority.set_cli_config(""),
                    { "strict", "disabled" },
                    channel_priority.description());

    auto& strict_channel_priority
        = config.insert(Configurable("strict_channel_priority", false)
                            .group("cli")
                            .rc_configurable(false)
                            .description("Enable strict channel priority")
                            .set_post_build_hook(strict_channel_priority_hook));
    subcom->add_flag("--strict-channel-priority",
                     strict_channel_priority.set_cli_config(0),
                     strict_channel_priority.description());

    auto& no_channel_priority = config.insert(Configurable("no_channel_priority", false)
                                                  .group("cli")
                                                  .rc_configurable(false)
                                                  .description("Disable channel priority")
                                                  .set_post_build_hook(no_channel_priority_hook));
    subcom->add_flag("--no-channel-priority",
                     no_channel_priority.set_cli_config(0),
                     no_channel_priority.description());
}

void
channels_hook(std::vector<std::string>& channels)
{
    auto& config = Configuration::instance();

    // Workaround to silent "override_channels_hook" warning
    // TODO: resolve configurable deps/workflow to compute only once
    auto verbosity = MessageLogger::global_log_level();
    MessageLogger::global_log_level() = mamba::LogLevel::kError;
    auto& override_channels = config.at("override_channels").compute_config().value<bool>();
    MessageLogger::global_log_level() = verbosity;

    if (override_channels)
    {
        channels = config.at("channels")
                       .compute_config(ConfigurationLevel::kCli, false)
                       .value<std::vector<std::string>>();
    }
}

void
override_channels_hook(bool& value)
{
    auto& config = Configuration::instance();
    auto& override_channels = config.at("override_channels");
    auto& override_channels_enabled
        = config.at("override_channels_enabled").compute_config().value<bool>();

    if (!override_channels_enabled && override_channels.configured())
    {
        LOG_WARNING
            << "'override_channels' disabled by 'override_channels_enabled' set to 'false' (skipped)";
        value = false;
    }
}

void
strict_channel_priority_hook(bool& value)
{
    auto& config = Configuration::instance();
    auto& channel_priority = config.at("channel_priority").get_wrapped<ChannelPriority>();
    auto& strict_channel_priority = config.at("strict_channel_priority");
    auto& no_channel_priority = config.at("no_channel_priority");

    if (strict_channel_priority.configured())
    {
        if ((channel_priority.cli_configured() || channel_priority.env_var_configured())
            && (channel_priority.compute_config().value() != ChannelPriority::kStrict))
        {
            throw std::runtime_error(
                "Cannot set both 'strict_channel_priority' and 'channel_priority'.");
        }
        else
        {
            if (no_channel_priority.configured())
            {
                throw std::runtime_error(
                    "Cannot set both 'strict_channel_priority' and 'no_channel_priority'.");
            }
            // Override 'channel_priority' CLI value
            channel_priority.set_cli_config("strict");
        }

        channel_priority.compute_config().set_context();
    }
}

void
no_channel_priority_hook(bool& value)
{
    auto& config = Configuration::instance();
    auto& channel_priority = config.at("channel_priority").get_wrapped<ChannelPriority>();
    auto& no_channel_priority = config.at("no_channel_priority");
    auto& strict_channel_priority = config.at("strict_channel_priority");

    if (no_channel_priority.configured())
    {
        if ((channel_priority.cli_configured() || channel_priority.env_var_configured())
            && (channel_priority.compute_config().value() != ChannelPriority::kDisabled))
        {
            throw std::runtime_error(
                "Cannot set both 'no_channel_priority' and 'channel_priority'.");
        }
        else
        {
            if (strict_channel_priority.configured())
            {
                throw std::runtime_error(
                    "Cannot set both 'no_channel_priority' and 'strict_channel_priority'.");
            }
            // Override 'channel_priority' CLI value
            channel_priority.set_cli_config("disabled");
        }

        channel_priority.compute_config().set_context();
    }
}


void
root_prefix_hook(fs::path& prefix)
{
    if (prefix.empty())
    {
        if (env::get("MAMBA_DEFAULT_ROOT_PREFIX").empty())
        {
            prefix = env::home_directory() / "micromamba";
            LOG_WARNING << "'root_prefix' set with default value: " << prefix.string();
        }
        else
        {
            prefix = env::get("MAMBA_DEFAULT_ROOT_PREFIX");
            LOG_WARNING << "'root_prefix' set with default value: " << prefix.string();
            LOG_WARNING << unindent(R"(
                            'MAMBA_DEFAULT_ROOT_PREFIX' is meant for testing purpose.
                            Consider using 'MAMBA_ROOT_PREFIX' instead)");
        }

        if (fs::exists(prefix) && !fs::is_empty(prefix))
        {
            if (!(fs::exists(prefix / "pkgs") || fs::exists(prefix / "conda-meta")))
            {
                LOG_ERROR << "Could not use default 'root_prefix' :" << prefix.string();
                LOG_ERROR << "Directory exists, is not empty and not a conda prefix.";
                exit(1);
            }
        }

        LOG_INFO << unindent(R"(
                    You have not set the 'root_prefix' environment variable.
                    To permanently modify the root prefix location, either:
                    - set the 'MAMBA_ROOT_PREFIX' environment variable
                    - use the '-r,--root-prefix' CLI option
                    - use 'micromamba shell init ...' to initialize your shell
                        (then restart or source the contents of the shell init script))");
    }

    auto& ctx = Context::instance();
    ctx.envs_dirs = { prefix / "envs" };
    ctx.pkgs_dirs = { prefix / "pkgs" };
}


void
check_target_prefix(int options)
{
    auto& ctx = Context::instance();
    auto& prefix = ctx.target_prefix;

    if (prefix.empty() && (options & MAMBA_ALLOW_FALLBACK_PREFIX))
    {
        prefix = std::getenv("CONDA_PREFIX") ? std::getenv("CONDA_PREFIX") : "";
    }

    if (prefix.empty())
    {
        if ((options & MAMBA_ALLOW_MISSING_PREFIX))
        {
            return;
        }
        else
        {
            LOG_ERROR << "No target prefix specified";
            exit(1);
        }
    }

    if (!(options & MAMBA_ALLOW_ROOT_PREFIX) && (prefix == ctx.root_prefix))
    {
        LOG_ERROR << "'root_prefix' not accepted as 'target_prefix'";
        exit(1);
    }

    bool allow_existing = options & MAMBA_ALLOW_EXISTING_PREFIX;

    if (!allow_existing && fs::exists(prefix))
    {
        if (prefix == ctx.root_prefix)
        {
            LOG_ERROR << "Overwriting root prefix is not permitted";
            throw std::runtime_error("Aborting.");
        }
        else if (fs::exists(prefix / "conda-meta"))
        {
            if (!allow_existing)
            {
                if (Console::prompt("Found conda-prefix at '" + prefix.string() + "'. Overwrite?",
                                    'n'))
                {
                    fs::remove_all(prefix);
                }
                else
                {
                    exit(1);
                }
            }
        }
        else
        {
            LOG_ERROR << "Non-conda folder exists at prefix";
            exit(1);
        }
    }
}


void
load_configuration(int options, bool show_banner)
{
    auto& config = Configuration::instance();

    config.at("no_env").compute_config().set_context();
    auto& no_rc = config.at("no_rc").compute_config().set_context().value<bool>();
    auto& rc_file = config.at("rc_file").compute_config().value<std::string>();

    config.at("quiet").compute_config().set_context();
    config.at("json").compute_config().set_context();
    config.at("always_yes").compute_config().set_context();
    config.at("offline").compute_config().set_context();
    config.at("dry_run").compute_config().set_context();

    if (show_banner)
        Console::print(banner);

    config.at("log_level").compute_config().set_context();
    config.at("verbose").compute_config();

    config.at("root_prefix").compute_config().set_context();
    config.at("target_prefix").compute_config().set_context();

    check_target_prefix(options);

    if (no_rc)
    {
        if (rc_file.empty())
        {
            Configuration::instance().load(std::vector<fs::path>({}));
        }
        else
        {
            LOG_ERROR << "'--no-rc' and '--rc-file' are exclusive";
            exit(1);
        }
    }
    else
    {
        if (rc_file.empty())
        {
            Configuration::instance().load();
        }
        else
        {
            Configuration::instance().load(rc_file);
        }
    }
    // Workaroud to rewrite correct target_prefix after loading..
    // TODO: fix this
    check_target_prefix(options);

    init_curl_ssl();
}
