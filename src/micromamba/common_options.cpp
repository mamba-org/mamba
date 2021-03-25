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
