// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "version.hpp"
#include "common_options.hpp"
#include "umamba.hpp"

#include "mamba/version.hpp"
#include "mamba/core/context.hpp"

#include "termcolor/termcolor.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_umamba_options(CLI::App* subcom)
{
    init_general_options(subcom);
    init_prefix_options(subcom);
}

void
set_umamba_command(CLI::App* com)
{
    init_umamba_options(com);

    Context::instance().caller_version = umamba::version();

    auto print_version = [](int count) {
        std::cout << "micromamba: " << umamba::version() << "\nlibmamba: " << mamba::version()
                  << std::endl;
        exit(0);
    };

    com->add_flag_function("--version", print_version);

    CLI::App* shell_subcom = com->add_subcommand("shell", "Generate shell init scripts");
    set_shell_command(shell_subcom);

    CLI::App* create_subcom = com->add_subcommand("create", "Create new environment");
    set_create_command(create_subcom);

    CLI::App* install_subcom
        = com->add_subcommand("install", "Install packages in active environment");
    set_install_command(install_subcom);

    CLI::App* update_subcom
        = com->add_subcommand("update", "Update packages in active environment");
    set_update_command(update_subcom);

    CLI::App* remove_subcom
        = com->add_subcommand("remove", "Remove packages from active environment");
    set_remove_command(remove_subcom);

    CLI::App* list_subcom = com->add_subcommand("list", "List packages in active environment");
    set_list_command(list_subcom);

    CLI::App* package_subcom
        = com->add_subcommand("package", "Extract a package or bundle files into an archive");
    set_package_command(package_subcom);

    CLI::App* clean_subcom = com->add_subcommand("clean", "Clean package cache");
    set_clean_command(clean_subcom);

    CLI::App* config_subcom = com->add_subcommand("config", "Configuration of micromamba");
    set_config_command(config_subcom);

    CLI::App* info_subcom = com->add_subcommand("info", "Information about micromamba");
    set_info_command(info_subcom);

    CLI::App* constructor_subcom
        = com->add_subcommand("constructor", "Commands to support using micromamba in constructor");
    set_constructor_command(constructor_subcom);

    CLI::App* env_subcom = com->add_subcommand("env", "List environments");
    set_env_command(env_subcom);

    std::stringstream footer;  // just for the help text

    footer
        << "In order to use activate and deactivate you need to initialize your shell.\n"
        << "Either call shell init ... or execute the shell hook directly:\n\n"
        << "    $ eval \"$(./micromamba shell hook -s bash)\"\n\n"
        << "and then run activate and deactivate like so:\n\n"
        << "    $ micromamba activate  " << termcolor::white
        << "# notice the missing dot in front of the command\n\n"
        << "To permanently initialize your shell, use shell init like so:\n\n"
        << "    $ ./micromamba shell init -s bash -p /home/$USER/micromamba \n\n"
        << "and restart your shell. Supported shells are bash, zsh, xonsh, cmd.exe, powershell, and fish."
        << termcolor::reset;

    com->footer(footer.str());
}
