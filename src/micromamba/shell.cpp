// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "shell.hpp"
#include "parsers.hpp"
#include "options.hpp"

#include "mamba/activation.hpp"
#include "mamba/shell_init.hpp"

#include "../thirdparty/termcolor.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_shell_parser(CLI::App* subcom)
{
    subcom->add_option("-s,--shell",
                       shell_options.shell_type,
                       "A shell type (bash, fish, posix, powershell, xonsh)");
    subcom->add_option("--stack",
                       shell_options.stack,
                       "Stack the environment being activated on top of the "
                       "previous active environment, "
                       "rather replacing the current active environment with a "
                       "new one. Currently, "
                       "only the PATH environment variable is stacked. "
                       "This may be enabled implicitly by the 'auto_stack' "
                       "configuration variable.");

    subcom->add_option("action", shell_options.action, "activate, deactivate or hook");
    // TODO add custom validator here!
    subcom->add_option("-p,--prefix",
                       shell_options.prefix,
                       "The root prefix to configure (for init and hook), and the prefix "
                       "to activate for activate, either by name or by path");
}


void
set_shell_command(CLI::App* subcom)
{
    init_shell_parser(subcom);

    subcom->callback([&]() {
        std::unique_ptr<Activator> activator;
        check_root_prefix(true);

        if (shell_options.shell_type.empty())
        {
            // Doesnt work yet.
            // std::string guessed_shell = guess_shell();
            // if (!guessed_shell.empty())
            // {
            //     // std::cout << "Guessing shell " << termcolor::green << guessed_shell <<
            //     // termcolor::reset << std::endl;
            //     shell_options.shell_type = guessed_shell;
            // }
        }

        if (shell_options.shell_type == "bash" || shell_options.shell_type == "zsh"
            || shell_options.shell_type == "posix")
        {
            activator = std::make_unique<mamba::PosixActivator>();
        }
        else if (shell_options.shell_type == "cmd.exe")
        {
            activator = std::make_unique<mamba::CmdExeActivator>();
        }
        else if (shell_options.shell_type == "powershell")
        {
            activator = std::make_unique<mamba::PowerShellActivator>();
        }
        else if (shell_options.shell_type == "xonsh")
        {
            activator = std::make_unique<mamba::XonshActivator>();
        }
        else
        {
            throw std::runtime_error(
                "Currently allowed values are: posix, bash, xonsh, zsh, cmd.exe & powershell");
        }
        if (shell_options.action == "init")
        {
            if (shell_options.prefix == "base")
            {
                shell_options.prefix = Context::instance().root_prefix;
            }
            init_shell(shell_options.shell_type, shell_options.prefix);
        }
        else if (shell_options.action == "hook")
        {
            // TODO do we need to do something wtih `prefix -> root_prefix?`?
            std::cout << activator->hook();
        }
        else if (shell_options.action == "activate")
        {
            if (shell_options.prefix == "base" || shell_options.prefix.empty())
            {
                shell_options.prefix = Context::instance().root_prefix;
            }
            // checking wether we have a path or a name
            if (shell_options.prefix.find_first_of("/\\") == std::string::npos)
            {
                shell_options.prefix
                    = Context::instance().root_prefix / "envs" / shell_options.prefix;
            }
            if (!fs::exists(shell_options.prefix))
            {
                throw std::runtime_error(
                    "Cannot activate, environment does not exist: " + shell_options.prefix + "\n");
            }
            std::cout << activator->activate(shell_options.prefix, shell_options.stack);
        }
        else if (shell_options.action == "reactivate")
        {
            std::cout << activator->reactivate();
        }
        else if (shell_options.action == "deactivate")
        {
            std::cout << activator->deactivate();
        }
        else
        {
            throw std::runtime_error("Need an action (activate, deactivate or hook)");
        }
    });
}
