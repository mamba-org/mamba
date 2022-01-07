// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"

#include "mamba/core/util.hpp"

using namespace mamba;  // NOLINT(build/namespaces)


void
set_activate_command(CLI::App* subcom)
{
    std::string name = "";
    bool stack = false;

    subcom->add_option("prefix", name, "The prefix to activate");
    subcom->add_flag("--stack", stack, "Defines if the activation has to be stacked or not");

    subcom->callback([&]() {
        std::string message = unindent(R"(
            In order to use activate and deactivate you need to initialize your shell.
            (micromamba is running as a subprocess and can't modify the parent one)

            To initialize the current (bash) shell, run:
                $ eval \"$(micromamba shell hook --shell=bash)\"
            and then activate or deactivate with:
                $ micromamba activate

            To automatically initialize all future (bash) shells, run:
                $ micromamba shell init --shell=bash --prefix=/home/$USER/micromamba

            Supported shells are bash, zsh, xonsh, cmd.exe, powershell, and fish.)");

        std::cout << "\n" << message << "\n" << std::endl;
        throw std::runtime_error("Shell not initialized");
    });
}
