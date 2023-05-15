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

#include "common_options.hpp"
#include "umamba.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

namespace
{
    void init_shell_parser(CLI::App* subcmd)
    {
        init_general_options(subcmd);

        auto& config = Configuration::instance();

        auto& shell_type = config.insert(
            Configurable("shell_type", std::string("")).group("cli").description("A shell type"),
            true
        );
        subcmd
            ->add_option("-s,--shell", shell_type.get_cli_config<std::string>(), shell_type.description())
            ->check(CLI::IsMember(std::set<std::string>(
                { "bash", "posix", "powershell", "cmd.exe", "xonsh", "zsh", "fish", "tcsh", "dash" }
            )));

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

    void set_shell_init_command(CLI::App* subsubcmd)
    {
        init_shell_parser(subsubcmd);
        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                shell_init(
                    consolidate_shell(config.at("shell_type").compute().value<std::string>()),
                    config.at("shell_prefix").compute().value<std::string>()
                );
                config.operation_teardown();
            }
        );
    }

    void set_shell_deinit_command(CLI::App* subsubcmd)
    {
        init_shell_parser(subsubcmd);
        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                shell_deinit(
                    consolidate_shell(config.at("shell_type").compute().value<std::string>()),
                    config.at("shell_prefix").compute().value<std::string>()
                );
                config.operation_teardown();
            }
        );
    }

    void set_shell_reinit_command(CLI::App* subsubcmd)
    {
        init_shell_parser(subsubcmd);
        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                shell_reinit(config.at("shell_prefix").compute().value<std::string>());
                config.operation_teardown();
            }
        );
    }

    void set_shell_hook_command(CLI::App* subsubcmd)
    {
        init_shell_parser(subsubcmd);
        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                shell_hook(consolidate_shell(config.at("shell_type").compute().value<std::string>()));
                config.operation_teardown();
            }
        );
    }

    void set_shell_activate_command(CLI::App* subsubcmd)
    {
        auto& config = Configuration::instance();
        init_shell_parser(subsubcmd);
        auto& stack = config.insert(Configurable("shell_stack", false)
                                        .group("cli")
                                        .description("Stack the environment being activated")
                                        .long_description(unindent(R"(
                       Stack the environment being activated on top of the
                       previous active environment, rather replacing the
                       current active environment with a new one.
                       Currently, only the PATH environment variable is stacked.
                       This may be enabled implicitly by the 'auto_stack'
                       configuration variable.)")));
        subsubcmd->add_flag("--stack", stack.get_cli_config<bool>(), stack.description());

        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                shell_activate(
                    config.at("shell_prefix").compute().value<std::string>(),
                    consolidate_shell(config.at("shell_type").compute().value<std::string>()),
                    config.at("shell_stack").compute().value<bool>()
                );
                config.operation_teardown();
            }
        );
    }

    void set_shell_reactivate_command(CLI::App* subsubcmd)
    {
        init_shell_parser(subsubcmd);
        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                shell_reactivate(
                    consolidate_shell(config.at("shell_type").compute().value<std::string>())
                );
                config.operation_teardown();
            }
        );
    }

    void set_shell_deactivate_command(CLI::App* subsubcmd)
    {
        init_shell_parser(subsubcmd);
        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                shell_deactivate(config.at("shell_type").compute().value<std::string>());
                config.operation_teardown();
            }
        );
    }

    void set_shell_long_path_command(CLI::App* subsubcmd)
    {
        init_shell_parser(subsubcmd);
        subsubcmd->callback(
            []()
            {
                auto& config = Configuration::instance();
                shell_enable_long_path_support();
                config.operation_teardown();
            }
        );
    }
}

void
set_shell_command(CLI::App* shell_subcmd)
{
    auto& config = Configuration::instance();
    config.at("show_banner").set_value(false);
    config.at("use_target_prefix_fallback").set_value(false);
    config.at("target_prefix_checks").set_value(MAMBA_NO_PREFIX_CHECK);
    config.load();

    // The initial parser had the subcmdmand as an action so both
    // ``micromamba shell init --shell bash`` and ``micromamba shell --shell bash init`` were
    // allowed.
    // We want to move everything to the first one, but keeping this for compatibility so far.
    init_shell_parser(shell_subcmd);

    set_shell_init_command(
        shell_subcmd->add_subcommand("init", "Add initialization in script to rc files")
    );

    set_shell_deinit_command(
        shell_subcmd->add_subcommand("deinit", "Remove activation script from rc files")
    );

    set_shell_reinit_command(
        shell_subcmd->add_subcommand("reinit", "Restore activation script from rc files")
    );

    set_shell_hook_command(shell_subcmd->add_subcommand("hook", "Micromamba hook scripts "));

    set_shell_activate_command(
        shell_subcmd->add_subcommand("activate", "Output activation code for the given shell")
    );

    set_shell_reactivate_command(
        shell_subcmd->add_subcommand("reactivate", "Output reactivateion code for the given shell")
    );

    set_shell_deactivate_command(
        shell_subcmd->add_subcommand("deactivate", "Output deactivation code for the given shell")
    );

    set_shell_long_path_command(shell_subcmd->add_subcommand(
        "enable_long_path_support",
        "Output deactivation code for the given shell"
    ));
}
