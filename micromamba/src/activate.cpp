// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"

#include "mamba/api/configuration.hpp"

#include "termcolor/termcolor.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_activate_parser(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    auto& env_name = config.insert(Configurable("activate_env_name", std::string(""))
                                       .group("cli")
                                       .description("The name of the environment to activate"));
    subcom->add_option("env_name", env_name.set_cli_config(""), env_name.description());
}

void
set_activate_command(CLI::App* subcom)
{
    init_activate_parser(subcom);

    subcom->callback([&]() {
        std::stringstream message;

        message
            << "\nIn order to use activate and deactivate you need to initialize your shell.\n"
            << "Either call shell init ... or execute the shell hook directly:\n\n"
            << "    $ eval \"$(./micromamba shell hook -s bash)\"\n\n"
            << "and then run activate and deactivate like so:\n\n"
            << "    $ micromamba activate  " << termcolor::white
            << "# notice the missing dot in front of the command\n\n"
            << "To permanently initialize your shell, use shell init like so:\n\n"
            << "    $ ./micromamba shell init -s bash -p /home/$USER/micromamba \n\n"
            << "and restart your shell. Supported shells are bash, zsh, xonsh, cmd.exe, powershell, and fish."
            << termcolor::reset;

        throw std::runtime_error(message.str());
    });
}
