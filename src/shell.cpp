// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/activation.hpp"
#include "mamba/configuration.hpp"
#include "mamba/context.hpp"
#include "mamba/mamba_fs.hpp"
#include "mamba/shell.hpp"
#include "mamba/shell_init.hpp"


namespace mamba
{
    void shell(const std::string& action, const std::string& shell_type, const fs::path& prefix)
    {
        auto& ctx = Context::instance();
        auto& config = Configuration::instance();
        auto& stack = config.at("shell_stack").value<bool>();
        std::unique_ptr<Activator> activator;
        fs::path shell_prefix = prefix;

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
            if (shell_prefix == "base")
            {
                shell_prefix = ctx.root_prefix;
            }
            init_shell(shell_type, shell_prefix);
        }
        else if (action == "hook")
        {
            // TODO do we need to do something wtih `prefix -> root_prefix?`?
            std::cout << activator->hook();
        }
        else if (action == "activate")
        {
            if (shell_prefix == "base" || shell_prefix.empty())
            {
                shell_prefix = ctx.root_prefix;
            }
            if (shell_prefix.string().find_first_of("/\\") == std::string::npos)
            {
                shell_prefix = ctx.root_prefix / "envs" / shell_prefix;
            }

            std::cout << activator->activate(shell_prefix, stack);
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
    }

}  // mamba
