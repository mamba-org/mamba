// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"

#include "mamba/core/shell_init.hpp"
#include "mamba/core/util.hpp"

using namespace mamba;  // NOLINT(build/namespaces)


void
set_activate_command(CLI::App* subcom)
{
    static std::string name = "";
    static bool stack = false;

    subcom->add_option("prefix", name, "The prefix to activate");
    subcom->add_flag(
        "--stack",
        stack,
        "Activate the specified environment without first deactivating the current one");

    subcom->callback(
        [&]()
        {
            std::string guessed_shell = guess_shell();
            std::string shell_hook_command = "", shell_hook = "";

            if (guessed_shell == "powershell")
                shell_hook_command
                    = "micromamba.exe shell hook -s powershell | Out-String | Invoke-Expression";
            else
                shell_hook_command
                    = "eval \"$(micromamba shell hook --shell=" + guessed_shell + ")\"";

            if (guessed_shell != "cmd.exe")
            {
                shell_hook = unindent((R"(

                To initialize the current )"
                                       + guessed_shell + R"( shell, run:
                    $ )" + shell_hook_command
                                       + R"(
                and then activate or deactivate with:
                    $ micromamba activate
                )")
                                          .c_str());
            }

            std::string message = unindent((R"(
            'micromamba' is running as a subprocess and can't modify the parent shell.
            Thus you must initialize your shell before using activate and deactivate.
            )" + shell_hook + R"(
            To automatically initialize all future ()"
                                            + guessed_shell + R"() shells, run:
                $ micromamba shell init --shell=)"
                                            + guessed_shell + R"( --prefix=~/micromamba

            Supported shells are {bash, zsh, csh, xonsh, cmd.exe, powershell, fish}.)")
                                               .c_str());

            std::cout << "\n" << message << "\n" << std::endl;
            throw std::runtime_error("Shell not initialized");
        });
}
