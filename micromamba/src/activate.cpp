// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <string_view>

#include <CLI/App.hpp>
#include <fmt/format.h>

#include "mamba/core/shell_init.hpp"
#include "mamba/core/util.hpp"

using namespace mamba;  // NOLINT(build/namespaces)

namespace
{
    auto get_shell_hook_command(std::string_view guessed_shell) -> std::string
    {
        if (guessed_shell == "powershell")
        {
            return "micromamba.exe shell hook -s powershell | Out-String | Invoke-Expression";
        }
        return fmt::format(R"sh(eval "$(micromamba shell hook --shell {})")sh", guessed_shell);
    };

    auto get_shell_hook(std::string_view guessed_shell) -> std::string
    {
        if (guessed_shell != "cmd.exe")
        {
            return fmt::format(
                "To initialize the current {} shell, run:\n"
                "    $ {}\nand then activate or deactivate with:\n"
                "    $ micromamba activate",
                guessed_shell,
                get_shell_hook_command(guessed_shell)
            );
        }
        return "";
    }
}

void
set_activate_command(CLI::App* subcom)
{
    static std::string name = "";
    static bool stack = false;

    subcom->add_option("prefix", name, "The prefix to activate");
    subcom->add_flag(
        "--stack",
        stack,
        "Activate the specified environment without first deactivating the current one"
    );

    subcom->callback(
        [&]()
        {
            std::string const guessed_shell = guess_shell();

            std::string const message = fmt::format(
                "\n'micromamba' is running as a subprocess and can't modify the parent shell.\n"
                "Thus you must initialize your shell before using activate and deactivate.\n"
                "\n"
                "{0}\n"
                "To automatically initialize all future ({1}) shells, run:\n"
                "    $ micromamba shell init --shell {1} --root-prefix=~/micromamba\n"
                "If your shell was already initialized, reinitialize your shell with:\n"
                "    $ micromamba shell reinit --shell {1}\n"
                "Otherwise, this may be an issue. In the meantime you can run commands. See:\n"
                "    $ micromamba run --help\n"
                "\n"
                "Supported shells are {{bash, zsh, csh, xonsh, cmd.exe, powershell, fish, nu}}.\n",
                get_shell_hook(guessed_shell),
                guessed_shell
            );

            std::cout << message;
            throw std::runtime_error("Shell not initialized");
        }
    );
}
