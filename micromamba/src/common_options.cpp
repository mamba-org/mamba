// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"

#include "mamba/api/configuration.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_rc_options(CLI::App* subcom)
{
    auto& config = Configuration::instance();
    std::string cli_group = "Configuration options";

    auto& rc_files = config.at("rc_files").get_wrapped<std::vector<fs::path>>();
    subcom->add_option("--rc-file", rc_files.set_cli_config({}), rc_files.description())
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

    auto& verbose = config.at("verbose").get_wrapped<std::uint8_t>();
    subcom
        ->add_flag("-v,--verbose",
                   verbose.set_cli_config(0),
                   "Set verbosity (higher verbosity with multiple -v, e.g. -vvv)")
        ->group(cli_group);

    auto& log_level = config.at("log_level").get_wrapped<spdlog::level::level_enum>();
    subcom->add_option("--log-level", log_level.set_cli_config(""), log_level.description())
        ->group(cli_group)
        ->check(CLI::IsMember(std::vector<std::string>(
            { "critical", "error", "warning", "info", "debug", "trace", "off" })));

    auto& quiet = config.at("quiet").get_wrapped<bool>();
    subcom->add_flag("-q,--quiet", quiet.set_cli_config(false), quiet.description())->group(cli_group);

    auto& always_yes = config.at("always_yes").get_wrapped<bool>();
    subcom->add_flag("-y,--yes", always_yes.set_cli_config(false), always_yes.description())
        ->group(cli_group);

    auto& json = config.at("json").get_wrapped<bool>();
    subcom->add_flag("--json", json.set_cli_config(false), json.description())->group(cli_group);

    auto& offline = config.at("offline").get_wrapped<bool>();
    subcom->add_flag("--offline", offline.set_cli_config(false), offline.description())
        ->group(cli_group);

    auto& dry_run = config.at("dry_run").get_wrapped<bool>();
    subcom->add_flag("--dry-run", dry_run.set_cli_config(false), dry_run.description())
        ->group(cli_group);

    auto& experimental = config.at("experimental").get_wrapped<bool>();
    subcom->add_flag("--experimental", experimental.set_cli_config(false), experimental.description())
        ->group(cli_group);

    auto& debug = config.at("debug").get_wrapped<bool>();
    subcom->add_flag("--debug", debug.set_cli_config(false), "Debug mode")->group("");

    auto& print_context_only = config.at("print_context_only").get_wrapped<bool>();
    subcom
        ->add_flag(
            "--print-context-only", print_context_only.set_cli_config(false), "Debug context")
        ->group("");

    auto& print_config_only = config.at("print_config_only").get_wrapped<bool>();
    subcom->add_flag("--print-config-only", print_config_only.set_cli_config(false), "Debug config")
        ->group("");
}

void
init_prefix_options(CLI::App* subcom)
{
    auto& config = Configuration::instance();
    std::string cli_group = "Prefix options";

    auto& root = config.at("root_prefix").get_wrapped<fs::path>();
    subcom->add_option("-r,--root-prefix", root.set_cli_config(""), root.description())
        ->group(cli_group);

    auto& prefix = config.at("target_prefix").get_wrapped<fs::path>();
    subcom->add_option("-p,--prefix", prefix.set_cli_config(""), prefix.description())
        ->group(cli_group);

    auto& name = config.at("env_name").get_wrapped<std::string>();
    subcom->add_option("-n,--name", name.set_cli_config(""), name.description())->group(cli_group);
}


void
init_network_options(CLI::App* subcom)
{
    auto& config = Configuration::instance();
    std::string cli_group = "Network options";

    auto& ssl_verify = config.at("ssl_verify").get_wrapped<std::string>();
    subcom->add_option("--ssl-verify", ssl_verify.set_cli_config(""), ssl_verify.description())
        ->group(cli_group);

    auto& ssl_no_revoke = config.at("ssl_no_revoke").get_wrapped<bool>();
    subcom
        ->add_flag("--ssl-no-revoke", ssl_no_revoke.set_cli_config(false), ssl_no_revoke.description())
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
    channels.set_post_merge_hook(channels_hook).needs({ "override_channels" });
    subcom->add_option("-c,--channel", channels.set_cli_config({}), channels.description())
        ->type_size(1)
        ->allow_extra_args(false);

    auto& override_channels = config.insert(Configurable("override_channels", false)
                                                .group("cli")
                                                .set_env_var_names()
                                                .description("Override channels")
                                                .needs({ "override_channels_enabled" })
                                                .set_post_merge_hook(override_channels_hook),
                                            true);
    subcom->add_flag("--override-channels",
                     override_channels.set_cli_config(false),
                     override_channels.description());

    auto& channel_priority = config.at("channel_priority").get_wrapped<ChannelPriority>();
    subcom
        ->add_option("--channel-priority",
                     channel_priority.set_cli_config(""),
                     channel_priority.description())
        ->check(CLI::IsMember(std::set<std::string>({ "strict", "flexible", "disabled" })));

    auto& channel_alias = config.at("channel_alias").get_wrapped<std::string>();
    subcom->add_option(
        "--channel-alias", channel_alias.set_cli_config(""), channel_alias.description());

    auto& strict_channel_priority
        = config.insert(Configurable("strict_channel_priority", false)
                            .group("cli")
                            .description("Enable strict channel priority")
                            .set_post_merge_hook(strict_channel_priority_hook),
                        true);
    subcom->add_flag("--strict-channel-priority",
                     strict_channel_priority.set_cli_config(false),
                     strict_channel_priority.description());

    auto& no_channel_priority = config.insert(Configurable("no_channel_priority", false)
                                                  .group("cli")
                                                  .description("Disable channel priority")
                                                  .set_post_merge_hook(no_channel_priority_hook),
                                              true);
    subcom->add_flag("--no-channel-priority",
                     no_channel_priority.set_cli_config(false),
                     no_channel_priority.description());

    channel_priority.needs({ "strict_channel_priority", "no_channel_priority" });
}

void
channels_hook(std::vector<std::string>& channels)
{
    auto& config = Configuration::instance();
    auto& override_channels = config.at("override_channels").value<bool>();

    if (override_channels)
    {
        if (config.at("channels").cli_configured())
        {
            channels = config.at("channels").cli_value<std::vector<std::string>>();
        }
        else
        {
            channels.clear();
        }
    }
}

void
override_channels_hook(bool& value)
{
    auto& config = Configuration::instance();
    auto& override_channels = config.at("override_channels");
    auto& override_channels_enabled = config.at("override_channels_enabled").value<bool>();

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
            && (channel_priority.cli_value() != ChannelPriority::kStrict))
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
            && (channel_priority.cli_value() != ChannelPriority::kDisabled))
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
    }
}

void
init_install_options(CLI::App* subcom)
{
    init_general_options(subcom);
    init_prefix_options(subcom);
    init_network_options(subcom);
    init_channel_parser(subcom);

    auto& config = Configuration::instance();

    auto& specs = config.at("specs").get_wrapped<std::vector<std::string>>();
    subcom->add_option("specs", specs.set_cli_config({}), "Specs to install into the environment");

    auto& file_specs = config.at("file_specs").get_wrapped<std::vector<std::string>>();
    subcom->add_option("-f,--file", file_specs.set_cli_config({}), file_specs.description())
        ->type_size(1)
        ->allow_extra_args(false);

    auto& no_pin = config.at("no_pin").get_wrapped<bool>();
    subcom->add_flag("--no-pin,!--pin", no_pin.set_cli_config(false), no_pin.description());

    auto& no_py_pin = config.at("no_py_pin").get_wrapped<bool>();
    subcom->add_flag("--no-py-pin,!--py-pin", no_py_pin.set_cli_config(false), no_py_pin.description());

    auto& compile_pyc = config.at("compile_pyc").get_wrapped<bool>();
    subcom->add_flag("--pyc,!--no-pyc", compile_pyc.set_cli_config(false), compile_pyc.description());

    auto& allow_uninstall = config.at("allow_uninstall").get_wrapped<bool>();
    subcom->add_flag("--allow-uninstall,!--no-allow-uninstall",
                     allow_uninstall.set_cli_config(false),
                     allow_uninstall.description());

    auto& allow_downgrade = config.at("allow_downgrade").get_wrapped<bool>();
    subcom->add_flag("--allow-downgrade,!--no-allow-downgrade",
                     allow_downgrade.set_cli_config(false),
                     allow_downgrade.description());

    auto& allow_softlinks = config.at("allow_softlinks").get_wrapped<bool>();
    subcom->add_flag("--allow-softlinks,!--no-allow-softlinks",
                     allow_softlinks.set_cli_config(false),
                     allow_softlinks.description());

    auto& always_softlink = config.at("always_softlink").get_wrapped<bool>();
    subcom->add_flag("--always-softlink,!--no-always-softlink",
                     always_softlink.set_cli_config(false),
                     always_softlink.description());

    auto& always_copy = config.at("always_copy").get_wrapped<bool>();
    subcom->add_flag("--always-copy,!--no-always-copy",
                     always_copy.set_cli_config(false),
                     always_copy.description());

    auto& extra_safety_checks = config.at("extra_safety_checks").get_wrapped<bool>();
    subcom->add_flag("--extra-safety-checks,!--no-extra-safety-checks",
                     extra_safety_checks.set_cli_config(false),
                     extra_safety_checks.description());

    auto& lock_timeout = config.at("lock_timeout").get_wrapped<std::size_t>();
    subcom->add_option(
        "--lock-timeout", lock_timeout.set_cli_config(-1), lock_timeout.description());

    auto& shortcuts = config.at("shortcuts").get_wrapped<bool>();
    subcom->add_flag(
        "--shortcuts,!--no-shortcuts", shortcuts.set_cli_config(false), shortcuts.description());

    auto& safety_checks = config.at("safety_checks").get_wrapped<VerificationLevel>();
    subcom
        ->add_option(
            "--safety-checks", safety_checks.set_cli_config(""), safety_checks.description())
        ->check(CLI::IsMember(std::set<std::string>({ "enabled", "warn", "disabled" })));

    auto& av = config.at("verify_artifacts").get_wrapped<bool>();
    subcom->add_flag("--verify-artifacts", av.set_cli_config(false), av.description());

    auto& platform = config.at("platform").get_wrapped<std::string>();
    subcom->add_option("--platform", platform.set_cli_config(""), platform.description());
}
