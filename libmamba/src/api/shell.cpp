// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#include "mamba/api/configuration.hpp"
#include "mamba/api/shell.hpp"

#include "mamba/core/activation.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/shell_init.hpp"

#ifdef _WIN32
#include "mamba/core/util_os.hpp"
#endif

namespace mamba
{
    void detect_shell(std::string& shell_type)
    {
        if (shell_type.empty())
        {
            LOG_DEBUG << "No shell type provided";

            std::string guessed_shell = guess_shell();
            if (!guessed_shell.empty())
            {
                LOG_DEBUG << "Guessed shell: '" << guessed_shell << "'";
                shell_type = guessed_shell;
            }

            if (shell_type.empty())
            {
                LOG_ERROR << "Please provide a shell type." << std::endl
                          << "Run with --help for more information." << std::endl;
                throw std::runtime_error("Unknown shell type. Aborting.");
            }
        }
    }

    void shell(const std::string& action,
               std::string& shell_type,
               const std::string& prefix,
               bool stack)
    {
        auto& ctx = Context::instance();
        auto& config = Configuration::instance();

        config.at("show_banner").set_value(false);
        config.at("use_target_prefix_fallback").set_value(false);
        config.at("target_prefix_checks").set_value(MAMBA_NO_PREFIX_CHECK);
        config.load();

        detect_shell(shell_type);

        std::unique_ptr<Activator> activator;
        std::string shell_prefix;

        if (shell_type == "bash" || shell_type == "zsh" || shell_type == "dash"
            || shell_type == "posix")
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
        else if (shell_type == "fish")
        {
            activator = std::make_unique<mamba::FishActivator>();
        }
        else
        {
            LOG_ERROR << "Not handled 'shell_type': " << shell_type;
            return;
        }

        if (action == "init")
        {
            if (prefix.empty() || prefix == "base")
                shell_prefix = ctx.root_prefix.string();
            else
                shell_prefix = fs::weakly_canonical(env::expand_user(prefix)).string();

            init_shell(shell_type, shell_prefix);
        }
        else if (action == "deinit")
        {
            if (prefix.empty() || prefix == "base")
                shell_prefix = ctx.root_prefix.string();
            else
                shell_prefix = fs::weakly_canonical(env::expand_user(prefix)).string();

            deinit_shell(shell_type, shell_prefix);
        }
        else if (action == "hook")
        {
            // TODO do we need to do something wtih `shell_prefix -> root_prefix?`?
            if (ctx.json)
            {
                Console::instance().json_write(
                    { { "success", true },
                      { "operation", "shell_hook" },
                      { "context", { { "shell_type", shell_type } } },
                      { "actions", { { "print", { activator->hook(shell_type) } } } } });
            }
            else
            {
                std::cout << activator->hook(shell_type);
            }
        }
        else if (action == "activate")
        {
            if (prefix.empty() || prefix == "base")
                shell_prefix = ctx.root_prefix.string();
            else if (prefix.find_first_of("/\\") == std::string::npos)
                shell_prefix = (ctx.root_prefix / "envs" / prefix).string();
            else
                shell_prefix = fs::weakly_canonical(env::expand_user(prefix)).string();

            if (!fs::exists(shell_prefix))
                throw std::runtime_error("Cannot activate, prefix does not exist at: "
                                         + shell_prefix);

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
#ifdef _WIN32
        else if (action == "enable-long-paths-support")
        {
            if (!enable_long_paths_support(true))
            {
                throw std::runtime_error("Error enabling Windows long-path support");
            }
        }
#endif
        else
        {
            throw std::runtime_error(
                "Need an action {init, hook, activate, deactivate, reactivate}");
        }

        config.operation_teardown();
    }
}  // mamba
