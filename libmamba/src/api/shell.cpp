// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#include <fmt/format.h>

#include "mamba/api/shell.hpp"
#include "mamba/core/activation.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/shell_init.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/path_manip.hpp"

#ifdef _WIN32
#include "mamba/core/util_os.hpp"
#endif

namespace mamba
{
    namespace
    {
        auto make_activator(const Context& context, std::string_view name)
            -> std::unique_ptr<Activator>
        {
            if (name == "bash" || name == "zsh" || name == "dash" || name == "posix")
            {
                return std::make_unique<mamba::PosixActivator>(context);
            }
            if (name == "csh" || name == "tcsh")
            {
                return std::make_unique<mamba::CshActivator>(context);
            }
            if (name == "cmd.exe")
            {
                return std::make_unique<mamba::CmdExeActivator>(context);
            }
            if (name == "powershell")
            {
                return std::make_unique<mamba::PowerShellActivator>(context);
            }
            if (name == "xonsh")
            {
                return std::make_unique<mamba::XonshActivator>(context);
            }
            if (name == "fish")
            {
                return std::make_unique<mamba::FishActivator>(context);
            }
            if (name == "nu")
            {
                return std::make_unique<mamba::NuActivator>(context);
            }
            throw std::invalid_argument(fmt::format("Shell type not handled: {}", name));
        }
    }

    void shell_init(Context& ctx, const std::string& shell_type, const fs::u8path& prefix)
    {
        if (prefix.empty() || prefix == "base")
        {
            init_shell(ctx, shell_type, ctx.prefix_params.root_prefix);
        }
        else
        {
            init_shell(ctx, shell_type, fs::weakly_canonical(util::expand_home(prefix.string())));
        }
    }

    void shell_deinit(Context& ctx, const std::string& shell_type, const fs::u8path& prefix)
    {
        if (prefix.empty() || prefix == "base")
        {
            deinit_shell(ctx, shell_type, ctx.prefix_params.root_prefix);
        }
        else
        {
            deinit_shell(ctx, shell_type, fs::weakly_canonical(util::expand_home(prefix.string())));
        }
    }

    void shell_reinit(Context& ctx, const fs::u8path& prefix)
    {
        // re-initialize all the shell scripts after update
        for (const auto& shell_type : find_initialized_shells())
        {
            shell_init(ctx, shell_type, prefix);
        }
    }

    void shell_hook(Context& ctx, const std::string& shell_type)
    {
        auto activator = make_activator(ctx, shell_type);
        // TODO do we need to do something wtih `shell_prefix -> root_prefix?`?
        if (ctx.output_params.json)
        {
            Console::instance().json_write({ { "success", true },
                                             { "operation", "shell_hook" },
                                             { "context", { { "shell_type", shell_type } } },
                                             { "actions",
                                               { { "print", { activator->hook(shell_type) } } } } });
        }
        else
        {
            std::cout << activator->hook(shell_type);
        }
    }

    void
    shell_activate(Context& ctx, const fs::u8path& prefix, const std::string& shell_type, bool stack)
    {
        if (!fs::exists(prefix))
        {
            throw std::runtime_error(
                fmt::format("Cannot activate, prefix does not exist at: {}", prefix)
            );
        }

        auto activator = make_activator(ctx, shell_type);
        std::cout << activator->activate(prefix, stack);
    }

    void shell_reactivate(Context& ctx, const std::string& shell_type)
    {
        auto activator = make_activator(ctx, shell_type);
        std::cout << activator->reactivate();
    }

    void shell_deactivate(Context& ctx, const std::string& shell_type)
    {
        auto activator = make_activator(ctx, shell_type);
        std::cout << activator->deactivate();
    }

    void shell_enable_long_path_support(Palette palette)
    {
#ifdef _WIN32
        if (const bool success = enable_long_paths_support(/* force= */ true, palette); !success)
        {
            throw std::runtime_error("Error enabling Windows long-path support");
        }
#else
        throw std::invalid_argument("Long path support is a Windows only option");
#endif
    }
}
