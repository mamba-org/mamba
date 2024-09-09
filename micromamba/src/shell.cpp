// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/shell.hpp"
#include "mamba/core/run.hpp"
#include "mamba/core/shell_init.hpp"
#include "mamba/util/build.hpp"
#include "mamba/util/environment.hpp"

#include "common_options.hpp"
#include "umamba.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

namespace
{
    /*****************
     *  CLI Options  *
     *****************/

    void init_shell_option(CLI::App* subcmd, Configuration& config)
    {
        auto& shell_type = config.insert(
            Configurable("shell_type", std::string("")).group("cli").description("A shell type"),
            true
        );
        subcmd
            ->add_option("-s,--shell", shell_type.get_cli_config<std::string>(), shell_type.description())
            ->check(CLI::IsMember(std::set<std::string>(
                { "bash", "posix", "powershell", "cmd.exe", "xonsh", "zsh", "fish", "tcsh", "dash", "nu" }
            )));
    }

    void init_root_prefix_option(CLI::App* subcmd, Configuration& config)
    {
        auto& root = config.at("root_prefix");
        subcmd->add_option(
            "root_prefix,-r,--root-prefix",
            root.get_cli_config<fs::u8path>(),
            root.description()
        );
    }

    void init_prefix_options(CLI::App* subcmd, Configuration& config)
    {
        auto& prefix = config.at("target_prefix");
        auto* prefix_cli = subcmd->add_option(
            "-p,--prefix",
            prefix.get_cli_config<fs::u8path>(),
            prefix.description()
        );

        auto& name = config.at("env_name");
        auto* name_cli = subcmd
                             ->add_option(
                                 "-n,--name",
                                 name.get_cli_config<std::string>(),
                                 name.description()
                             )
                             ->excludes(prefix_cli);

        auto& prefix_or_name = config.insert(
            Configurable("prefix_or_name", std::string(""))
                .group("cli")
                .description("The prefix to activate, either by name or by path"),
            true
        );

        subcmd
            ->add_option(
                "prefix_or_name",
                prefix_or_name.get_cli_config<std::string>(),
                prefix_or_name.description()
            )
            ->excludes(prefix_cli)
            ->excludes(name_cli);
    }

    void init_stack_option(CLI::App* subcmd, Configuration& config)
    {
        auto& stack = config.insert(
            Configurable("shell_stack", false)
                .group("cli")
                .description("Stack the environment being activated")
                .long_description(
                    "Stack the environment being activated on top of the previous active"
                    " environment, rather replacing the current active environment with a new one."
                    " Currently, only the PATH environment variable is stacked."
                    " This may be enabled implicitly by the 'auto_stack' configuration variable."
                )
        );
        subcmd->add_flag("--stack", stack.get_cli_config<bool>(), stack.description());
    }

    /***************
     *  Utilities  *
     ***************/

    void set_default_config_options(Configuration& config)
    {
        config.at("use_target_prefix_fallback").set_value(false);
        config.at("use_root_prefix_fallback").set_value(false);
        config.at("target_prefix_checks").set_value(MAMBA_NO_PREFIX_CHECK);
    }

    auto consolidate_shell(std::string_view shell_type) -> std::string
    {
        if (!shell_type.empty())
        {
            return std::string{ shell_type };
        }

        LOG_DEBUG << "No shell type provided";

        if (std::string guessed_shell = guess_shell(); !guessed_shell.empty())
        {
            LOG_DEBUG << "Guessed shell: '" << guessed_shell << "'";
            return guessed_shell;
        }

        LOG_ERROR << "Please provide a shell type.\n"
                     "Run with --help for more information.\n";
        throw std::runtime_error("Unknown shell type. Aborting.");
    }

    void consolidate_prefix_options(Configuration& config)
    {
        auto& p_opt = config.at("target_prefix");
        auto& n_opt = config.at("env_name");
        auto& pn_opt = config.at("prefix_or_name");

        // The prefix or name was passed without an explicit `-n` or `-p`, we make an assumption
        if (pn_opt.cli_configured())
        {
            auto prefix_or_name = pn_opt.compute().value<std::string>();
            if (prefix_or_name.find_first_of("/\\") != std::string::npos)
            {
                config.at("target_prefix").set_cli_value(fs::u8path(std::move(prefix_or_name)));
            }
            else if (!prefix_or_name.empty())
            {
                config.at("env_name").set_cli_value(std::move(prefix_or_name));
            }
        }
        // Nothing was given, `micromamba activate` means `mircomamba activate -n base`
        else if (!p_opt.configured() && !n_opt.configured())
        {
            config.at("env_name").set_cli_value(std::string("base"));
        }
    }

    /****************************
     *  Shell sub sub commands  *
     ****************************/

    void set_shell_init_command(CLI::App* subsubcmd, Configuration& config)
    {
        init_general_options(subsubcmd, config);
        init_shell_option(subsubcmd, config);
        init_root_prefix_option(subsubcmd, config);
        subsubcmd->callback(
            [&config]()
            {
                auto& context = config.context();
                set_default_config_options(config);
                config.load();
                shell_init(
                    context,
                    consolidate_shell(config.at("shell_type").compute().value<std::string>()),
                    context.prefix_params.root_prefix
                );
                config.operation_teardown();
            }
        );
    }

    void set_shell_deinit_command(CLI::App* subsubcmd, Configuration& config)
    {
        init_general_options(subsubcmd, config);
        init_shell_option(subsubcmd, config);
        init_root_prefix_option(subsubcmd, config);
        subsubcmd->callback(
            [&config]()
            {
                auto& context = config.context();
                set_default_config_options(config);
                config.load();
                shell_deinit(
                    context,
                    consolidate_shell(config.at("shell_type").compute().value<std::string>()),
                    context.prefix_params.root_prefix
                );
                config.operation_teardown();
            }
        );
    }

    void set_shell_reinit_command(CLI::App* subsubcmd, Configuration& config)
    {
        init_general_options(subsubcmd, config);
        subsubcmd->callback(
            [&config]()
            {
                auto& context = config.context();
                set_default_config_options(config);
                config.load();
                shell_reinit(context, context.prefix_params.root_prefix);
                config.operation_teardown();
            }
        );
    }

    void set_shell_hook_command(CLI::App* subsubcmd, Configuration& config)
    {
        init_general_options(subsubcmd, config);
        init_shell_option(subsubcmd, config);
        init_root_prefix_option(subsubcmd, config);  // FIXME not used here set in CLI scripts
        subsubcmd->callback(
            [&config]()
            {
                auto& context = config.context();
                set_default_config_options(config);
                config.load();
                shell_hook(
                    context,
                    consolidate_shell(config.at("shell_type").compute().value<std::string>())
                );
                config.operation_teardown();
            }
        );
    }

    void set_shell_activate_command(CLI::App* subsubcmd, Configuration& config)
    {
        init_general_options(subsubcmd, config);
        init_shell_option(subsubcmd, config);
        init_prefix_options(subsubcmd, config);
        init_stack_option(subsubcmd, config);

        subsubcmd->callback(
            [&config]()
            {
                auto& context = config.context();
                set_default_config_options(config);
                consolidate_prefix_options(config);
                config.load();
                shell_activate(
                    context,
                    context.prefix_params.target_prefix,
                    consolidate_shell(config.at("shell_type").compute().value<std::string>()),
                    config.at("shell_stack").compute().value<bool>()
                );
                config.operation_teardown();
            }
        );
    }

    void set_shell_reactivate_command(CLI::App* subsubcmd, Configuration& config)
    {
        init_general_options(subsubcmd, config);
        init_shell_option(subsubcmd, config);
        subsubcmd->callback(
            [&config]()
            {
                auto& context = config.context();
                set_default_config_options(config);
                config.load();
                shell_reactivate(
                    context,
                    consolidate_shell(config.at("shell_type").compute().value<std::string>())
                );
                config.operation_teardown();
            }
        );
    }

    void set_shell_deactivate_command(CLI::App* subsubcmd, Configuration& config)
    {
        init_general_options(subsubcmd, config);
        init_shell_option(subsubcmd, config);
        subsubcmd->callback(
            [&config]()
            {
                auto& context = config.context();
                set_default_config_options(config);
                config.load();
                shell_deactivate(context, config.at("shell_type").compute().value<std::string>());
                config.operation_teardown();
            }
        );
    }

    void set_shell_long_path_command(CLI::App* subsubcmd, mamba::Configuration& config)
    {
        init_general_options(subsubcmd, config);
        subsubcmd->callback(
            [&config]()
            {
                set_default_config_options(config);
                config.load();
                shell_enable_long_path_support(config.context().graphics_params.palette);
                config.operation_teardown();
            }
        );
    }

    /***********************
     *  Shell sub command  *
     ***********************/

    template <typename Arr>
    void
    set_shell_launch_command(CLI::App* subcmd, const Arr& all_subsubcmds, mamba::Configuration& config)
    {
        // The initial parser had the subcmdmand as an action so both
        // ``micromamba shell init --shell bash`` and ``micromamba shell --shell bash init`` were
        // allowed.
        init_general_options(subcmd, config);
        init_prefix_options(subcmd, config);

        subcmd->callback(
            [all_subsubcmds, &config]()
            {
                const bool got_subsubcmd = std::any_of(
                    all_subsubcmds.cbegin(),
                    all_subsubcmds.cend(),
                    [](auto* subsubcmd) -> bool { return subsubcmd->parsed(); }
                );
                // It is important to not do anything before that (not even loading the config)
                // because this callback may be greedily executed, even with a sub sub command.
                if (!got_subsubcmd)
                {
                    set_default_config_options(config);
                    consolidate_prefix_options(config);
                    config.load();

                    const auto get_shell = []() -> std::string
                    {
                        if constexpr (util::on_win)
                        {
                            return util::get_env("SHELL").value_or("cmd.exe");
                        }
                        else if constexpr (util::on_mac)
                        {
                            return util::get_env("SHELL").value_or("zsh");
                        }
                        return util::get_env("SHELL").value_or("bash");
                    };

                    exit(mamba::run_in_environment(
                        config.context(),
                        config.context().prefix_params.target_prefix,
                        { get_shell() },
                        ".",
                        static_cast<int>(STREAM_OPTIONS::ALL_STREAMS),
                        false,
                        false,
                        {},
                        ""
                    ));
                }
            }
        );
    }
}

void
set_shell_command(CLI::App* shell_subcmd, Configuration& config)
{
    auto* init_subsubcmd = shell_subcmd->add_subcommand(
        "init",
        "Add initialization in script to rc files"
    );
    set_shell_init_command(init_subsubcmd, config);

    auto* deinit_subsubcmd = shell_subcmd->add_subcommand(
        "deinit",
        "Remove activation script from rc files"
    );
    set_shell_deinit_command(deinit_subsubcmd, config);

    auto* reinit_subsubcmd = shell_subcmd->add_subcommand(
        "reinit",
        "Restore activation script from rc files"
    );
    set_shell_reinit_command(reinit_subsubcmd, config);

    auto* hook_subsubcmd = shell_subcmd->add_subcommand("hook", "Micromamba hook scripts ");
    set_shell_hook_command(hook_subsubcmd, config);

    auto* acti_subsubcmd = shell_subcmd->add_subcommand(
        "activate",
        "Output activation code for the given shell"
    );
    set_shell_activate_command(acti_subsubcmd, config);

    auto* reacti_subsubcmd = shell_subcmd->add_subcommand(
        "reactivate",
        "Output reactivateion code for the given shell"
    );
    set_shell_reactivate_command(reacti_subsubcmd, config);

    auto* deacti_subsubcmd = shell_subcmd->add_subcommand(
        "deactivate",
        "Output deactivation code for the given shell"
    );
    set_shell_deactivate_command(deacti_subsubcmd, config);

    auto* long_path_subsubcmd = shell_subcmd->add_subcommand(
        "enable_long_path_support",
        "Output deactivation code for the given shell"
    );
    set_shell_long_path_command(long_path_subsubcmd, config);

    // `micromamba shell` is used to launch a new shell
    // TODO micromamba 2.0 rename this command (e.g. start-shell) or the other to avoid
    // confusion between `micromamba shell` and `micromamba shell subsubcmd`.
    const auto all_subsubcmds = std::array{
        init_subsubcmd, deinit_subsubcmd, reinit_subsubcmd, hook_subsubcmd,
        acti_subsubcmd, reacti_subsubcmd, deacti_subsubcmd, long_path_subsubcmd,
    };
    set_shell_launch_command(shell_subcmd, all_subsubcmds, config);
}
