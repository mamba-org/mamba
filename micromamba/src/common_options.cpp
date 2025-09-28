// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"
#include "mamba/api/configuration.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_rc_options(CLI::App* subcom, Configuration& config)
{
    std::string cli_group = "Configuration options";

    auto& rc_files = config.at("rc_files");
    subcom
        ->add_option(
            "--rc-file",
            rc_files.get_cli_config<std::vector<fs::u8path>>(),
            rc_files.description()
        )
        ->option_text("FILE1 FILE2...")
        ->group(cli_group);

    auto& no_rc = config.at("no_rc");
    subcom->add_flag("--no-rc", no_rc.get_cli_config<bool>(), no_rc.description())->group(cli_group);

    auto& no_env = config.at("no_env");
    subcom->add_flag("--no-env", no_env.get_cli_config<bool>(), no_env.description())->group(cli_group);
}

void
init_general_options(CLI::App* subcom, Configuration& config)
{
    init_rc_options(subcom, config);

    std::string cli_group = "Global options";

    auto& verbose = config.at("verbose");
    subcom
        ->add_flag(
            "-v,--verbose",
            verbose.get_cli_config<int>(),
            "Set verbosity (higher verbosity with multiple -v, e.g. -vvv)"
        )
        ->multi_option_policy(CLI::MultiOptionPolicy::Sum)
        ->group(cli_group);

    std::map<std::string, mamba::log_level> le_map = { { "critical", mamba::log_level::critical },
                                                       { "error", mamba::log_level::err },
                                                       { "warning", mamba::log_level::warn },
                                                       { "info", mamba::log_level::info },
                                                       { "debug", mamba::log_level::debug },
                                                       { "trace", mamba::log_level::trace },
                                                       { "off", mamba::log_level::off } };
    auto& log_level = config.at("log_level");
    subcom
        ->add_option("--log-level", log_level.get_cli_config<mamba::log_level>(), log_level.description())
        ->group(cli_group)
        ->transform(CLI::CheckedTransformer(le_map, CLI::ignore_case));

    auto& quiet = config.at("quiet");
    subcom->add_flag("-q,--quiet", quiet.get_cli_config<bool>(), quiet.description())->group(cli_group);

    auto& always_yes = config.at("always_yes");
    subcom->add_flag("-y,--yes", always_yes.get_cli_config<bool>(), always_yes.description())
        ->group(cli_group);

    auto& json = config.at("json");
    subcom->add_flag("--json", json.get_cli_config<bool>(), json.description())->group(cli_group);

    auto& offline = config.at("offline");
    subcom->add_flag("--offline", offline.get_cli_config<bool>(), offline.description())->group(cli_group);

    auto& dry_run = config.at("dry_run");
    subcom->add_flag("--dry-run", dry_run.get_cli_config<bool>(), dry_run.description())->group(cli_group);

    auto& download_only = config.at("download_only");
    subcom
        ->add_flag("--download-only", download_only.get_cli_config<bool>(), download_only.description())
        ->group(cli_group);

    auto& experimental = config.at("experimental");
    subcom
        ->add_flag("--experimental", experimental.get_cli_config<bool>(), experimental.description())
        ->group(cli_group);

    auto& use_uv = config.at("use_uv");
    subcom->add_flag("--use-uv", use_uv.get_cli_config<bool>(), use_uv.description())->group(cli_group);

    auto& debug = config.at("debug");
    subcom->add_flag("--debug", debug.get_cli_config<bool>(), "Debug mode")->group("");

    auto& print_context_only = config.at("print_context_only");
    subcom
        ->add_flag("--print-context-only", print_context_only.get_cli_config<bool>(), "Debug context")
        ->group("");

    auto& print_config_only = config.at("print_config_only");
    subcom
        ->add_flag("--print-config-only", print_config_only.get_cli_config<bool>(), "Debug config")
        ->group("");
}

void
init_prefix_options(CLI::App* subcom, Configuration& config)
{
    std::string cli_group = "Prefix options";

    auto& root = config.at("root_prefix");
    subcom->add_option("-r,--root-prefix", root.get_cli_config<fs::u8path>(), root.description())
        ->option_text("PATH")
        ->group(cli_group);

    auto& prefix = config.at("target_prefix");
    subcom->add_option("-p,--prefix", prefix.get_cli_config<fs::u8path>(), prefix.description())
        ->option_text("PATH")
        ->group(cli_group);

    auto& relocate_prefix = config.at("relocate_prefix");
    subcom
        ->add_option(
            "--relocate-prefix",
            relocate_prefix.get_cli_config<fs::u8path>(),
            relocate_prefix.description()
        )
        ->option_text("PATH")
        ->group(cli_group);

    auto& name = config.at("env_name");
    subcom->add_option("-n,--name", name.get_cli_config<std::string>(), name.description())
        ->option_text("NAME")
        ->group(cli_group);
}

void
init_network_options(CLI::App* subcom, Configuration& config)
{
    std::string cli_group = "Network options";

    auto& ssl_verify = config.at("ssl_verify");
    subcom
        ->add_option("--ssl-verify", ssl_verify.get_cli_config<std::string>(), ssl_verify.description())
        ->option_text("'<false>' or PATH")
        ->group(cli_group);

    auto& ssl_no_revoke = config.at("ssl_no_revoke");
    subcom
        ->add_flag("--ssl-no-revoke", ssl_no_revoke.get_cli_config<bool>(), ssl_no_revoke.description())
        ->group(cli_group);

    auto& cacert_path = config.at("cacert_path");
    subcom
        ->add_option(
            "--cacert-path",
            cacert_path.get_cli_config<std::string>(),
            cacert_path.description()
        )
        ->option_text("PATH")
        ->group(cli_group);

    auto& local_repodata_ttl = config.at("local_repodata_ttl");
    subcom
        ->add_option(
            "--repodata-ttl",
            local_repodata_ttl.get_cli_config<std::size_t>(),
            local_repodata_ttl.description()
        )
        ->group(cli_group);

    auto& retry_clean_cache = config.at("retry_clean_cache");
    subcom
        ->add_flag(
            "--retry-clean-cache",
            retry_clean_cache.get_cli_config<bool>(),
            retry_clean_cache.description()
        )
        ->group(cli_group);
}

void
init_channel_parser(CLI::App* subcom, Configuration& config)
{
    using string_list = std::vector<std::string>;

    auto& channels = config.at("channels");
    channels.needs({ "override_channels" });
    subcom
        ->add_option("-c,--channel", channels.get_cli_config<string_list>(), channels.description())
        ->option_text("CHANNEL")
        ->type_size(1)
        ->allow_extra_args(false);

    auto& override_channels = config.insert(
        Configurable("override_channels", false)
            .group("cli")
            .set_env_var_names()
            .description("Override channels")
            .needs({ "override_channels_enabled" })
            .set_post_merge_hook<bool>([&](bool& value)
                                       { return override_channels_hook(config, value); }),
        true
    );
    subcom->add_flag(
        "--override-channels",
        override_channels.get_cli_config<bool>(),
        override_channels.description()
    );

    std::map<std::string, ChannelPriority> cp_map = { { "disabled", ChannelPriority::Disabled },
                                                      { "flexible", ChannelPriority::Flexible },
                                                      { "strict", ChannelPriority::Strict } };
    auto& channel_priority = config.at("channel_priority");
    subcom
        ->add_option(
            "--channel-priority",
            channel_priority.get_cli_config<ChannelPriority>(),
            channel_priority.description()
        )
        ->transform(CLI::CheckedTransformer(cp_map, CLI::ignore_case));

    auto& channel_alias = config.at("channel_alias");
    subcom
        ->add_option(
            "--channel-alias",
            channel_alias.get_cli_config<std::string>(),
            channel_alias.description()
        )
        ->option_text("URL");

    auto& strict_channel_priority = config.insert(
        Configurable("strict_channel_priority", false)
            .group("cli")
            .description("Enable strict channel priority")
            .set_post_merge_hook<bool>([&](bool& value)
                                       { return strict_channel_priority_hook(config, value); }),
        true
    );
    subcom->add_flag(
        "--strict-channel-priority",
        strict_channel_priority.get_cli_config<bool>(),
        strict_channel_priority.description()
    );

    auto& no_channel_priority = config.insert(
        Configurable("no_channel_priority", false)
            .group("cli")
            .description("Disable channel priority")
            .set_post_merge_hook<bool>([&](bool& value)
                                       { return no_channel_priority_hook(config, value); }),
        true
    );
    subcom->add_flag(
        "--no-channel-priority",
        no_channel_priority.get_cli_config<bool>(),
        no_channel_priority.description()
    );

    channel_priority.needs({ "strict_channel_priority", "no_channel_priority" });
}

void
override_channels_hook(Configuration& config, bool& value)
{
    auto& override_channels = config.at("override_channels");
    auto& channels = config.at("channels");
    bool override_channels_enabled = config.at("override_channels_enabled").value<bool>();

    if (!override_channels_enabled && override_channels.configured())
    {
        LOG_WARNING
            << "'override_channels' disabled by 'override_channels_enabled' set to 'false' (skipped)";
        value = false;
    }

    if (value)
    {
        std::vector<std::string> updated_channels;
        if (channels.cli_configured())
        {
            updated_channels = channels.cli_value<std::vector<std::string>>();
        }
        updated_channels.emplace_back("nodefaults");
        channels.set_cli_value(updated_channels);
    }
}

void
strict_channel_priority_hook(Configuration& config, bool&)
{
    auto& channel_priority = config.at("channel_priority");
    auto& strict_channel_priority = config.at("strict_channel_priority");
    auto& no_channel_priority = config.at("no_channel_priority");

    if (strict_channel_priority.configured())
    {
        if ((channel_priority.cli_configured() || channel_priority.env_var_configured())
            && (channel_priority.cli_value<ChannelPriority>() != ChannelPriority::Strict))
        {
            throw std::runtime_error("Cannot set both 'strict_channel_priority' and 'channel_priority'."
            );
        }
        else
        {
            if (no_channel_priority.configured())
            {
                throw std::runtime_error(
                    "Cannot set both 'strict_channel_priority' and 'no_channel_priority'."
                );
            }
            // Override 'channel_priority' CLI value
            channel_priority.set_cli_value(ChannelPriority::Strict);
        }
    }
}

void
no_channel_priority_hook(Configuration& config, bool&)
{
    auto& channel_priority = config.at("channel_priority");
    auto& no_channel_priority = config.at("no_channel_priority");
    auto& strict_channel_priority = config.at("strict_channel_priority");

    if (no_channel_priority.configured())
    {
        if ((channel_priority.cli_configured() || channel_priority.env_var_configured())
            && (channel_priority.cli_value<ChannelPriority>() != ChannelPriority::Disabled))
        {
            throw std::runtime_error("Cannot set both 'no_channel_priority' and 'channel_priority'.");
        }
        else
        {
            if (strict_channel_priority.configured())
            {
                throw std::runtime_error(
                    "Cannot set both 'no_channel_priority' and 'strict_channel_priority'."
                );
            }
            // Override 'channel_priority' CLI value
            channel_priority.set_cli_value(ChannelPriority::Disabled);
        }
    }
}

void
init_install_options(CLI::App* subcom, Configuration& config)
{
    using string_list = std::vector<std::string>;
    init_general_options(subcom, config);
    init_prefix_options(subcom, config);
    init_network_options(subcom, config);
    init_channel_parser(subcom, config);

    auto& specs = config.at("specs");
    subcom
        ->add_option("specs", specs.get_cli_config<string_list>(), "Specs to install into the environment")
        ->option_text("SPECS");

    auto& file_specs = config.at("file_specs");
    subcom
        ->add_option("-f,--file", file_specs.get_cli_config<string_list>(), file_specs.description())
        ->option_text("FILE")
        ->type_size(1)
        ->allow_extra_args(false);

    auto& clone_env = config.at("clone_env");
    subcom
        ->add_option("--clone", clone_env.get_cli_config<std::string>(), clone_env.description())
        ->option_text("ENV_NAME_OR_PATH");

    auto& no_pin = config.at("no_pin");
    subcom->add_flag("--no-pin,!--pin", no_pin.get_cli_config<bool>(), no_pin.description());

    auto& no_py_pin = config.at("no_py_pin");
    subcom->add_flag("--no-py-pin,!--py-pin", no_py_pin.get_cli_config<bool>(), no_py_pin.description());

    auto& compile_pyc = config.at("compile_pyc");
    subcom->add_flag("--pyc,!--no-pyc", compile_pyc.get_cli_config<bool>(), compile_pyc.description());

    auto& allow_uninstall = config.at("allow_uninstall");
    subcom->add_flag(
        "--allow-uninstall,!--no-allow-uninstall",
        allow_uninstall.get_cli_config<bool>(),
        allow_uninstall.description()
    );

    auto& allow_downgrade = config.at("allow_downgrade");
    subcom->add_flag(
        "--allow-downgrade,!--no-allow-downgrade",
        allow_downgrade.get_cli_config<bool>(),
        allow_downgrade.description()
    );

    auto& allow_softlinks = config.at("allow_softlinks");
    subcom->add_flag(
        "--allow-softlinks,!--no-allow-softlinks",
        allow_softlinks.get_cli_config<bool>(),
        allow_softlinks.description()
    );

    auto& always_softlink = config.at("always_softlink");
    subcom->add_flag(
        "--always-softlink,!--no-always-softlink",
        always_softlink.get_cli_config<bool>(),
        always_softlink.description()
    );

    auto& always_copy = config.at("always_copy");
    subcom->add_flag(
        "--always-copy,--copy,!--no-always-copy",
        always_copy.get_cli_config<bool>(),
        always_copy.description()
    );

    auto& lock_timeout = config.at("lock_timeout");
    subcom->add_option(
        "--lock-timeout",
        lock_timeout.get_cli_config<std::size_t>(),
        lock_timeout.description()
    );

    auto& shortcuts = config.at("shortcuts");
    subcom->add_flag(
        "--shortcuts,!--no-shortcuts",
        shortcuts.get_cli_config<bool>(),
        shortcuts.description()
    );

    std::map<std::string, VerificationLevel> vl_map = { { "enabled", VerificationLevel::Enabled },
                                                        { "warn", VerificationLevel::Warn },
                                                        { "disabled", VerificationLevel::Disabled } };
    auto& safety_checks = config.at("safety_checks");
    subcom
        ->add_option(
            "--safety-checks",
            safety_checks.get_cli_config<VerificationLevel>(),
            safety_checks.description()
        )
        ->transform(CLI::CheckedTransformer(vl_map, CLI::ignore_case));

    auto& extra_safety_checks = config.at("extra_safety_checks");
    subcom->add_flag(
        "--extra-safety-checks,!--no-extra-safety-checks",
        extra_safety_checks.get_cli_config<bool>(),
        extra_safety_checks.description()
    );

    auto& av = config.at("verify_artifacts");
    subcom->add_flag("--verify-artifacts", av.get_cli_config<bool>(), av.description());

    auto& trusted_channels = config.at("trusted_channels");
    // Allowing unlimited number of args (may be modified later if needed using `type_size` and
    // `allow_extra_args`)
    subcom
        ->add_option(
            "--trusted-channels",
            trusted_channels.get_cli_config<string_list>(),
            trusted_channels.description()
        )
        ->option_text("CHANNEL1 CHANNEL2...");

    auto& repo_parsing = config.at("experimental_repodata_parsing");
    subcom->add_flag(
        "--exp-repodata-parsing, !--no-exp-repodata-parsing",
        repo_parsing.get_cli_config<bool>(),
        repo_parsing.description()
    );

    auto& platform = config.at("platform");
    subcom->add_option("--platform", platform.get_cli_config<std::string>(), platform.description())
        ->option_text("PLATFORM");

    auto& no_deps = config.at("no_deps");
    subcom->add_flag("--no-deps", no_deps.get_cli_config<bool>(), no_deps.description());
    auto& only_deps = config.at("only_deps");
    subcom->add_flag("--only-deps", only_deps.get_cli_config<bool>(), only_deps.description());

    auto& categories = config.at("categories");
    subcom
        ->add_option(
            "--category",
            categories.get_cli_config<string_list>(),
            "Categories of package to install from environment lockfile"
        )
        ->option_text("CAT1 CAT2...");
}
