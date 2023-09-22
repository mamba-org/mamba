// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/version.hpp"

#include "common_options.hpp"
#include "umamba.hpp"
#include "version.hpp"

using namespace mamba;  // NOLINT(build/namespaces)

void
init_umamba_options(CLI::App* subcom, Configuration& config)
{
    init_general_options(subcom, config);
    init_prefix_options(subcom, config);
}

void
set_umamba_command(CLI::App* com, mamba::Configuration& config)
{
    init_umamba_options(com, config);

    auto& context = config.context();

    context.command_params.caller_version = umamba::version();

    auto print_version = [](int /*count*/)
    {
        std::cout << umamba::version() << std::endl;
        exit(0);
    };

    com->add_flag_function("--version", print_version);

    CLI::App* shell_subcom = com->add_subcommand("shell", "Generate shell init scripts");
    set_shell_command(shell_subcom, config);

    CLI::App* create_subcom = com->add_subcommand("create", "Create new environment");
    set_create_command(create_subcom, config);

    CLI::App* install_subcom = com->add_subcommand("install", "Install packages in active environment");
    set_install_command(install_subcom, config);

    CLI::App* update_subcom = com->add_subcommand("update", "Update packages in active environment");
    set_update_command(update_subcom, config);

    CLI::App* self_update_subcom = com->add_subcommand("self-update", "Update micromamba");
    set_self_update_command(self_update_subcom, config);

    CLI::App* repoquery_subcom = com->add_subcommand(
        "repoquery",
        "Find and analyze packages in active environment or channels"
    );
    set_repoquery_command(repoquery_subcom, config);

    CLI::App* remove_subcom = com->add_subcommand("remove", "Remove packages from active environment");
    set_remove_command(remove_subcom, config);

    CLI::App* list_subcom = com->add_subcommand("list", "List packages in active environment");
    set_list_command(list_subcom, config);

    CLI::App* package_subcom = com->add_subcommand(
        "package",
        "Extract a package or bundle files into an archive"
    );
    set_package_command(package_subcom, config);

    CLI::App* clean_subcom = com->add_subcommand("clean", "Clean package cache");
    set_clean_command(clean_subcom, config);

    CLI::App* config_subcom = com->add_subcommand("config", "Configuration of micromamba");
    set_config_command(config_subcom, config);

    CLI::App* info_subcom = com->add_subcommand("info", "Information about micromamba");
    set_info_command(info_subcom, config);

    CLI::App* constructor_subcom = com->add_subcommand(
        "constructor",
        "Commands to support using micromamba in constructor"
    );
    set_constructor_command(constructor_subcom, config);

    CLI::App* env_subcom = com->add_subcommand("env", "List environments");
    set_env_command(env_subcom, config);

    CLI::App* activate_subcom = com->add_subcommand("activate", "Activate an environment");
    set_activate_command(activate_subcom);

    CLI::App* run_subcom = com->add_subcommand("run", "Run an executable in an environment");
    set_run_command(run_subcom, config);

    CLI::App* ps_subcom = com->add_subcommand("ps", "Show, inspect or kill running processes");
    set_ps_command(ps_subcom, context);

    CLI::App* auth_subcom = com->add_subcommand("auth", "Login or logout of a given host");
    set_auth_command(auth_subcom);

    CLI::App* search_subcom = com->add_subcommand(
        "search",
        "Find packages in active environment or channels\n"
        "This is equivalent to `repoquery search` command"
    );
    set_search_command(search_subcom, config);

#if !defined(_WIN32) && defined(MICROMAMBA_SERVER)
    CLI::App* server_subcom = com->add_subcommand("server", "Run micromamba server");
    set_server_command(server_subcom, config);
#endif

    com->require_subcommand(/* min */ 0, /* max */ 1);
}
