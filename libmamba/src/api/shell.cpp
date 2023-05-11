// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#include <fmt/format.h>

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
    namespace
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

        auto make_activator(std::string_view name) -> std::unique_ptr<Activator>
        {
            if (name == "bash" || name == "zsh" || name == "dash" || name == "posix")
            {
                return std::make_unique<mamba::PosixActivator>();
            }
            if (name == "csh" || name == "tcsh")
            {
                return std::make_unique<mamba::CshActivator>();
            }
            if (name == "cmd.exe")
            {
                return std::make_unique<mamba::CmdExeActivator>();
            }
            if (name == "powershell")
            {
                return std::make_unique<mamba::PowerShellActivator>();
            }
            if (name == "xonsh")
            {
                return std::make_unique<mamba::XonshActivator>();
            }
            if (name == "fish")
            {
                return std::make_unique<mamba::FishActivator>();
            }
            throw std::invalid_argument(fmt::format("Shell type not handled: {}", name));
        }
    }

    void shell_init(const std::string& shell_type, std::string_view prefix)
    {
        auto& ctx = Context::instance();
        if (prefix.empty() || prefix == "base")
        {
            init_shell(shell_type, ctx.prefix_params.root_prefix);
        }
        else
        {
            init_shell(shell_type, fs::weakly_canonical(env::expand_user(prefix)));
        }
    }

    void shell_deinit(const std::string& shell_type, std::string_view prefix)
    {
        auto& ctx = Context::instance();
        if (prefix.empty() || prefix == "base")
        {
            deinit_shell(shell_type, ctx.prefix_params.root_prefix);
        }
        else
        {
            deinit_shell(shell_type, fs::weakly_canonical(env::expand_user(prefix)));
        }
    }

    void shell_reinit(std::string_view prefix)
    {
        // re-initialize all the shell scripts after update
        for (const auto& shell_type : find_initialized_shells())
        {
            shell_init(shell_type, prefix);
        }
    }

    void shell_hook(const std::string& shell_type, Activator& activator)
    {
        auto& ctx = Context::instance();
        // TODO do we need to do something wtih `shell_prefix -> root_prefix?`?
        if (ctx.output_params.json)
        {
            Console::instance().json_write({ { "success", true },
                                             { "operation", "shell_hook" },
                                             { "context", { { "shell_type", shell_type } } },
                                             { "actions",
                                               { { "print", { activator.hook(shell_type) } } } } });
        }
        else
        {
            std::cout << activator.hook(shell_type);
        }
    }

    void shell_activate(std::string_view prefix, Activator& activator, bool stack)
    {
        auto& ctx = Context::instance();
        fs::u8path shell_prefix;
        if (prefix.empty() || prefix == "base")
        {
            shell_prefix = ctx.prefix_params.root_prefix;
        }
        else if (prefix.find_first_of("/\\") == std::string::npos)
        {
            shell_prefix = ctx.prefix_params.root_prefix / "envs" / prefix;
        }
        else
        {
            shell_prefix = fs::weakly_canonical(env::expand_user(prefix));
        }

        if (!fs::exists(shell_prefix))
        {
            throw std::runtime_error(
                fmt::format("Cannot activate, prefix does not exist at: {}", shell_prefix)
            );
        }

        std::cout << activator.activate(shell_prefix, stack);
    }

    void shell_reactivate(Activator& activator)
    {
        std::cout << activator.deactivate();
    }

    void shell_deactivate(Activator& activator)
    {
        std::cout << activator.deactivate();
    }
    void shell_enable_long_path_support()
    {
#ifdef _WIN32
        if (const bool success = enable_long_paths_support(/* force= */ true); !success)
        {
            throw std::runtime_error("Error enabling Windows long-path support");
        }
#else
        throw std::invalid_argument("Long path support is a Windows only option");
#endif
    }

    void
    shell(const std::string& action, std::string& shell_type, const std::string& prefix, bool stack)
    {
        auto& config = Configuration::instance();

        config.at("show_banner").set_value(false);
        config.at("use_target_prefix_fallback").set_value(false);
        config.at("target_prefix_checks").set_value(MAMBA_NO_PREFIX_CHECK);
        config.load();

        detect_shell(shell_type);

        auto activator = make_activator(shell_type);

        std::string shell_prefix;

        if (action == "init")
        {
            shell_init(shell_type, prefix);
        }
        else if (action == "deinit")
        {
            shell_deinit(shell_type, prefix);
        }
        else if (action == "reinit")
        {
            shell_reinit(prefix);
        }
        else if (action == "hook")
        {
            shell_hook(shell_type, *activator);
        }
        else if (action == "activate")
        {
            shell_activate(prefix, *activator, stack);
        }
        else if (action == "reactivate")
        {
            shell_reactivate(*activator);
        }
        else if (action == "deactivate")
        {
            shell_deactivate(*activator);
        }
        else if (action == "enable-long-paths-support")
        {
            shell_enable_long_path_support();
        }
        else
        {
            throw std::runtime_error("Need an action {init, hook, activate, deactivate, reactivate}");
        }

        config.operation_teardown();
    }
}  // mamba
