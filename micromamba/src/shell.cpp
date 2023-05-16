// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/shell.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/run.hpp"
#include "mamba/core/shell_init.hpp"
#include "mamba/core/util_string.hpp"

#include "common_options.hpp"
#include "umamba.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

namespace
{
    /*****************
     *  CLI options  *
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
                { "bash", "posix", "powershell", "cmd.exe", "xonsh", "zsh", "fish", "tcsh", "dash" }
            )));
    }

    void init_root_prefix_option(CLI::App* subcmd, Configuration& config)
    {
        auto& prefix = config.insert(
            Configurable("shell_prefix", std::string(""))
                .group("cli")
                .description("The root prefix to configure (for init and hook), and the prefix "
                             "to activate for activate, either by name or by path"),
            true
        );
        subcmd->add_option(
            "prefix,-p,--prefix,-n,--name",
            prefix.get_cli_config<std::string>(),
            prefix.description()
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
                .description("The prefix to activate, either by name or by path")
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

    /**********************
     *  Sub sub commands  *
     **********************/

    void set_shell_init_command(CLI::App* subsubcmd, Configuration& config)
    {
        init_general_options(subsubcmd);
        init_shell_option(subsubcmd, config);
        init_root_prefix_option(subsubcmd, config);
        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                config.load();
                shell_init(
                    consolidate_shell(config.at("shell_type").compute().value<std::string>()),
                    config.at("shell_prefix").compute().value<std::string>()
                );
                config.operation_teardown();
            }
        );
    }

    void set_shell_deinit_command(CLI::App* subsubcmd, Configuration& config)
    {
        init_general_options(subsubcmd);
        init_shell_option(subsubcmd, config);
        init_root_prefix_option(subsubcmd, config);
        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                config.load();
                shell_deinit(
                    consolidate_shell(config.at("shell_type").compute().value<std::string>()),
                    config.at("shell_prefix").compute().value<std::string>()
                );
                config.operation_teardown();
            }
        );
    }

    void set_shell_reinit_command(CLI::App* subsubcmd, Configuration& config)
    {
        init_general_options(subsubcmd);
        init_shell_option(subsubcmd, config);
        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                config.load();
                shell_reinit(config.at("shell_prefix").compute().value<std::string>());
                config.operation_teardown();
            }
        );
    }

    void set_shell_hook_command(CLI::App* subsubcmd, Configuration& config)
    {
        init_general_options(subsubcmd);
        init_shell_option(subsubcmd, config);
        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                config.load();
                shell_hook(consolidate_shell(config.at("shell_type").compute().value<std::string>()));
                config.operation_teardown();
            }
        );
    }

    void set_shell_activate_command(CLI::App* subsubcmd, Configuration& config)
    {
        init_general_options(subsubcmd);
        init_shell_option(subsubcmd, config);
        init_prefix_options(subsubcmd, config);
        init_stack_option(subsubcmd, config);

        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                consolidate_prefix_options(config);
                config.load();

                shell_activate(
                    Context::instance().prefix_params.target_prefix,
                    consolidate_shell(config.at("shell_type").compute().value<std::string>()),
                    config.at("shell_stack").compute().value<bool>()
                );
                config.operation_teardown();
            }
        );
    }

    void set_shell_reactivate_command(CLI::App* subsubcmd, Configuration& config)
    {
        init_general_options(subsubcmd);
        init_shell_option(subsubcmd, config);
        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                config.load();
                shell_reactivate(
                    consolidate_shell(config.at("shell_type").compute().value<std::string>())
                );
                config.operation_teardown();
            }
        );
    }

    void set_shell_deactivate_command(CLI::App* subsubcmd, Configuration& config)
    {
        init_general_options(subsubcmd);
        init_shell_option(subsubcmd, config);
        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                config.load();
                shell_deactivate(config.at("shell_type").compute().value<std::string>());
                config.operation_teardown();
            }
        );
    }

    void set_shell_long_path_command(CLI::App* subsubcmd, Configuration& /* config */)
    {
        init_general_options(subsubcmd);
        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                config.load();
                shell_enable_long_path_support();
                config.operation_teardown();
            }
        );
    }
}

/***********************
 *  Shell sub command  *
 ***********************/

void
set_shell_command(CLI::App* shell_subcmd)
{
    auto& config = Configuration::instance();
    config.at("show_banner").set_value(false);
    config.at("use_target_prefix_fallback").set_value(false);
    config.at("target_prefix_checks").set_value(MAMBA_NO_PREFIX_CHECK);

    // The initial parser had the subcmdmand as an action so both
    // ``micromamba shell init --shell bash`` and ``micromamba shell --shell bash init`` were
    // allowed.
    init_general_options(shell_subcmd);
    init_shell_option(shell_subcmd, config);

    set_shell_init_command(
        shell_subcmd->add_subcommand("init", "Add initialization in script to rc files"),
        config
    );

    set_shell_deinit_command(
        shell_subcmd->add_subcommand("deinit", "Remove activation script from rc files"),
        config
    );

    set_shell_reinit_command(
        shell_subcmd->add_subcommand("reinit", "Restore activation script from rc files"),
        config
    );

    set_shell_hook_command(shell_subcmd->add_subcommand("hook", "Micromamba hook scripts "), config);

    set_shell_activate_command(
        shell_subcmd->add_subcommand("activate", "Output activation code for the given shell"),
        config
    );

    set_shell_reactivate_command(
        shell_subcmd->add_subcommand("reactivate", "Output reactivateion code for the given shell"),
        config
    );

    set_shell_deactivate_command(
        shell_subcmd->add_subcommand("deactivate", "Output deactivation code for the given shell"),
        config
    );

    set_shell_long_path_command(
        shell_subcmd->add_subcommand(
            "enable_long_path_support",
            "Output deactivation code for the given shell"
        ),
        config
    );
}
