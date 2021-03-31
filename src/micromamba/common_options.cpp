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
    auto& ctx = Context::instance();
    auto& config = Configuration::instance();
    std::string cli_group = "Configuration options";

    auto& rc_file = config.insert(Configurable("rc_file", std::string(""))
                                      .group("cli")
                                      .rc_configurable(false)
                                      .set_env_var_name()
                                      .description("Path to the unique configuration file to use"));
    subcom->add_option("--rc-file", rc_file.set_cli_config(""), rc_file.description())
        ->group(cli_group);

    auto& no_rc = config.insert(Configurable("no_rc", &ctx.no_rc)
                                    .group("cli")
                                    .rc_configurable(false)
                                    .set_env_var_name()
                                    .description("Disable the use of configuration files"));
    subcom->add_flag("--no-rc", no_rc.set_cli_config(0), no_rc.description())->group(cli_group);

    auto& no_env = config.insert(Configurable("no_env", &ctx.no_env)
                                     .group("cli")
                                     .rc_configurable(false)
                                     .set_env_var_name()
                                     .description("Disable the use of environment variables"));
    subcom->add_flag("--no-env", no_env.set_cli_config(0), no_env.description())->group(cli_group);
}


void
init_general_options(CLI::App* subcom)
{
    init_rc_options(subcom);

    auto& ctx = Context::instance();
    auto& config = Configuration::instance();
    std::string cli_group = "Global options";

    auto& verbosity = config.insert(
        Configurable("verbosity", 0)
            .group("cli")
            .rc_configurable(false)
            .set_env_var_name()
            .description("Set verbosity (higher verbosity with multiple -v, e.g. -vvv)"));
    subcom->add_flag("-v,--verbose", verbosity.set_cli_config(0), verbosity.description())
        ->group(cli_group);

    auto& quiet = config.insert(Configurable("quiet", &ctx.quiet)
                                    .group("cli")
                                    .rc_configurable(false)
                                    .set_env_var_name()
                                    .description("Set quiet mode (print less output)"));
    subcom->add_flag("-q,--quiet", quiet.set_cli_config(false), quiet.description())
        ->group(cli_group);

    auto& always_yes
        = config.insert(Configurable("always_yes", &ctx.always_yes)
                            .group("cli")
                            .rc_configurable(false)
                            .set_env_var_name()
                            .description("Automatically answer yes on prompted questions"));
    subcom->add_flag("-y,--yes", always_yes.set_cli_config(false), always_yes.description())
        ->group(cli_group);

    auto& json = config.insert(Configurable("json", &ctx.json)
                                   .group("cli")
                                   .rc_configurable(false)
                                   .set_env_var_name()
                                   .description("Report all output as json"));
    subcom->add_flag("--json", json.set_cli_config(false), json.description())->group(cli_group);

    auto& offline = config.insert(Configurable("offline", &ctx.offline)
                                      .group("cli")
                                      .rc_configurable(false)
                                      .set_env_var_name()
                                      .description("Force use cached repodata"));
    subcom->add_flag("--offline", offline.set_cli_config(false), offline.description())
        ->group(cli_group);

    auto& dry_run = config.insert(Configurable("dry_run", &ctx.dry_run)
                                      .group("cli")
                                      .rc_configurable(false)
                                      .set_env_var_name()
                                      .description("Only display what would have been done"));
    subcom->add_flag("--dry-run", dry_run.set_cli_config(false), dry_run.description())
        ->group(cli_group);

#ifdef ENABLE_CONTEXT_DEBUGGING
    static bool debug_context = false;
    auto& print_context_only = config.insert(Configurable("print_context_only", &debug_context));
    subcom
        ->add_flag(
            "--print-context-only", print_context_only.set_cli_config(false), "Debug context")
        ->group(cli_group);
#endif
}

void
init_prefix_options(CLI::App* subcom)
{
    auto& config = Configuration::instance();
    std::string cli_group = "Prefix options";

    auto& root = config.at("root_prefix").get_wrapped<fs::path>();
    auto& prefix = config.at("target_prefix").get_wrapped<fs::path>();
    auto& name = config.at("env_name").get_wrapped<std::string>();

    subcom->add_option("-r,--root-prefix", root.set_cli_config(""), root.description())
        ->group(cli_group);
    subcom->add_option("-p,--prefix", prefix.set_cli_config(""), prefix.description())
        ->group(cli_group);
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
        ->add_flag("--ssl-no-revoke", ssl_no_revoke.set_cli_config(0), ssl_no_revoke.description())
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

    auto& retry_clean_cache
        = config.insert(Configurable("retry_clean_cache", false)
                            .group("cli")
                            .rc_configurable(false)
                            .set_env_var_name()
                            .description("If solve fails, try to fetch updated repodata"));
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

    auto& channel_alias = config.at("channel_alias").get_wrapped<std::string>();
    subcom->add_option(
        "--channel-alias", channel_alias.set_cli_config(""), channel_alias.description());

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

    // Workaround to silent the hook warning
    // TODO: resolve configurable deps/workflow to compute only once
    auto verbosity = MessageLogger::global_log_severity();
    MessageLogger::global_log_severity() = mamba::LogSeverity::kError;
    auto& override_channels = config.at("override_channels").compute().value<bool>();
    MessageLogger::global_log_severity() = verbosity;

    if (override_channels)
    {
        channels = config.at("channels")
                       .compute(ConfigurationLevel::kCli, false)
                       .value<std::vector<std::string>>();
    }
}

void
override_channels_hook(bool& value)
{
    auto& config = Configuration::instance();
    auto& override_channels = config.at("override_channels");
    auto& override_channels_enabled
        = config.at("override_channels_enabled").compute().value<bool>();

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
            && (channel_priority.compute().value() != ChannelPriority::kStrict))
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

        channel_priority.compute().set_context();
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
            && (channel_priority.compute().value() != ChannelPriority::kDisabled))
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

        channel_priority.compute().set_context();
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
    subcom->add_flag("--no-pin,!--pin", no_pin.set_cli_config(0), no_pin.description());

    auto& allow_softlinks = config.at("allow_softlinks").get_wrapped<bool>();
    subcom->add_flag("--allow-softlinks,!--no-allow-softlinks",
                     allow_softlinks.set_cli_config(0),
                     allow_softlinks.description());

    auto& always_softlink = config.at("always_softlink").get_wrapped<bool>();
    subcom->add_flag("--always-softlink,!--no-always-softlink",
                     always_softlink.set_cli_config(0),
                     always_softlink.description());

    auto& always_copy = config.at("always_copy").get_wrapped<bool>();
    subcom->add_flag("--always-copy,!--no-always-copy",
                     always_copy.set_cli_config(0),
                     always_copy.description());

    auto& extra_safety_checks = config.at("extra_safety_checks").get_wrapped<bool>();
    subcom->add_flag("--extra-safety-checks,!--no-extra-safety-checks",
                     extra_safety_checks.set_cli_config(0),
                     extra_safety_checks.description());

    auto& safety_checks = config.at("safety_checks").get_wrapped<VerificationLevel>();
    subcom->add_set("--safety-checks",
                    safety_checks.set_cli_config(""),
                    { "enabled", "warn", "disabled" },
                    safety_checks.description());
}
