// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/shell.hpp"

#include "mamba/core/activation.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/shell_init.hpp"
#include "mamba/core/util_os.hpp"


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

        std::string shell_prefix = env::expand_user(prefix);
        std::unique_ptr<Activator> activator;

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
            if (shell_prefix == "base")
            {
                shell_prefix = ctx.root_prefix;
            }
            if (shell_prefix.empty())
            {
                LOG_ERROR << "Empty prefix";
                throw std::runtime_error("Calling shell init with empty prefix");
            }
            init_shell(shell_type, shell_prefix);
        }
        else if (action == "hook")
        {
            // TODO do we need to do something wtih `shell_prefix -> root_prefix?`?
            if (ctx.json)
            {
                JsonLogger::instance().json_write(
                    { { "success", true },
                      { "operation", "shell_hook" },
                      { "context", { { "shell_type", shell_type } } },
                      { "actions", { { "print", { activator->hook() } } } } });
                if (Context::instance().json)
                    Console::instance().print(JsonLogger::instance().json_log.unflatten().dump(4),
                                              true);
            }
            else
            {
                std::cout << activator->hook();
            }
        }
        else if (action == "activate")
        {
            if (shell_prefix == "base" || shell_prefix.empty())
            {
                shell_prefix = ctx.root_prefix;
            }
            if (shell_prefix.find_first_of("/\\") == std::string::npos)
            {
                shell_prefix = ctx.root_prefix / "envs" / shell_prefix;
            }
            if (!fs::exists(shell_prefix))
            {
                throw std::runtime_error("Cannot activate, prefix does not exist at: "
                                         + shell_prefix);
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
#ifdef _WIN32
        else if (action == "enable-long-paths-support")
        {
            if (!enable_long_paths_support(true))
            {
                exit(1);
            }
        }
#endif
        else if (action == "completion")
        {
            if (shell_type == "bash")
            {
            }
            else
            {
                LOG_ERROR << "Shell auto-completion is not supported in '" << shell_type << "'";
                throw std::runtime_error("Shell auto-completion is not supported in '" + shell_type
                                         + "'");
            }
        }
        else
        {
            throw std::runtime_error("Need an action (activate, deactivate or hook)");
        }

        config.operation_teardown();
    }
}  // mamba
