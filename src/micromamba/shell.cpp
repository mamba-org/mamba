// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "shell.hpp"
#include "common_options.hpp"

#include "mamba/activation.hpp"
#include "mamba/configuration.hpp"
#include "mamba/shell_init.hpp"

#include "../thirdparty/termcolor.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_shell_parser(CLI::App* subcom)
{
    init_general_options(subcom);

    auto& config = Configuration::instance();

    auto& shell_type = config.insert(Configurable("shell_type", std::string(""))
                                         .group("cli")
                                         .rc_configurable(false)
                                         .description("A shell type"));
    subcom->add_set("-s,--shell",
                    shell_type.set_cli_config(""),
                    { "bash", "posix", "powershell", "cmd.exe", "xonsh", "zsh" },
                    shell_type.description());

    auto& stack = config.insert(Configurable("shell_stack", false)
                                    .group("cli")
                                    .rc_configurable(false)
                                    .description("Stack the environment being activated")
                                    .long_description(unindent(R"(
                       Stack the environment being activated on top of the
                       previous active environment, rather replacing the
                       current active environment with a new one.
                       Currently, only the PATH environment variable is stacked.
                       This may be enabled implicitly by the 'auto_stack'
                       configuration variable.)")));
    subcom->add_option("--stack", stack.set_cli_config(false), stack.description());

    auto& action = config.insert(Configurable("shell_action", std::string(""))
                                     .group("cli")
                                     .rc_configurable(false)
                                     .description("The action to complete"));
    subcom->add_set("action",
                    action.set_cli_config(""),
                    { "init", "activate", "deactivate", "hook", "reactivate", "deactivate" },
                    action.description());

    auto& prefix = config.insert(
        Configurable("shell_prefix", std::string(""))
            .group("cli")
            .rc_configurable(false)
            .description("The root prefix to configure (for init and hook), and the prefix "
                         "to activate for activate, either by name or by path"));
    subcom->add_option("-p,--prefix", prefix.set_cli_config(""), prefix.description());

    auto& auto_activate_base = config.at("auto_activate_base").get_wrapped<bool>();
    subcom->add_flag("--auto-activate-base,!--no-auto-activate-base",
                     auto_activate_base.set_cli_config(0),
                     auto_activate_base.description());
}


void
set_shell_command(CLI::App* subcom)
{
    init_shell_parser(subcom);

    subcom->callback([&]() {
        auto& ctx = Context::instance();
        auto& config = Configuration::instance();

        config.at("show_banner").get_wrapped<bool>().set_value(false);
        config.load(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                    | MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX);

        std::unique_ptr<Activator> activator;
        auto& shell_type = config.at("shell_type").value<std::string>();
        auto& action = config.at("shell_action").value<std::string>();
        std::string prefix = config.at("shell_prefix").value<std::string>();
        auto& stack = config.at("shell_stack").value<bool>();

        if (shell_type.empty())
        {
            std::cout << "Please provide a shell type." << std::endl;
            std::cout << "Run with --help for more information." << std::endl;
            return;
            // Doesnt work yet.
            // std::string guessed_shell = guess_shell();
            // if (!guessed_shell.empty())
            // {
            //     // std::cout << "Guessing shell " << termcolor::green << guessed_shell <<
            //     // termcolor::reset << std::endl;
            //     shell_options.shell_type = guessed_shell;
            // }
        }

        if (shell_type == "bash" || shell_type == "zsh" || shell_type == "posix")
        {
            activator = std::make_unique<mamba::PosixActivator>();
        }
        else if (shell_type == "cmd.exe")
        {
            activator = std::make_unique<mamba::CmdExeActivator>();
        }
        else if (shell_type == "powershell")
        {
            activator = std::make_unique<mamba::PowerShellActivator>();
        }
        else if (shell_type == "xonsh")
        {
            activator = std::make_unique<mamba::XonshActivator>();
        }
        else
        {
            throw std::runtime_error("Not handled 'shell_type'");
        }

        if (action == "init")
        {
            if (prefix == "base")
            {
                prefix = ctx.root_prefix;
            }
            init_shell(shell_type, prefix);
        }
        else if (action == "hook")
        {
            // TODO do we need to do something wtih `prefix -> root_prefix?`?
            std::cout << activator->hook();
        }
        else if (action == "activate")
        {
            if (prefix == "base" || prefix.empty())
            {
                prefix = ctx.root_prefix;
            }
            if (prefix.find_first_of("/\\") == std::string::npos)
            {
                prefix = ctx.root_prefix / "envs" / prefix;
            }

            std::cout << activator->activate(prefix, stack);
        }
        else if (action == "reactivate")
        {
            std::cout << activator->reactivate();
        }
        else if (action == "deactivate")
        {
            std::cout << activator->deactivate();
        }
        else
        {
            throw std::runtime_error("Need an action (activate, deactivate or hook)");
        }
    });
}
