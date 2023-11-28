// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_UMAMBA_HPP
#define UMAMBA_UMAMBA_HPP

#include <CLI/CLI.hpp>

namespace mamba
{
    class Configuration;
}


void
set_clean_command(CLI::App* subcom, mamba::Configuration& config);

void
set_config_command(CLI::App* subcom, mamba::Configuration& config);

void
set_constructor_command(CLI::App* subcom, mamba::Configuration& config);

void
set_create_command(CLI::App* subcom, mamba::Configuration& config);

void
set_info_command(CLI::App* subcom, mamba::Configuration& config);

void
set_install_command(CLI::App* subcom, mamba::Configuration& config);

void
set_list_command(CLI::App* subcom, mamba::Configuration& config);

void
set_remove_command(CLI::App* subcom, mamba::Configuration& config);

void
set_shell_command(CLI::App* subcom, mamba::Configuration& config);

void
set_package_command(CLI::App* subcom, mamba::Configuration& config);

void
set_umamba_command(CLI::App* com, mamba::Configuration& config);

void
set_update_command(CLI::App* subcom, mamba::Configuration& config);

void
set_self_update_command(CLI::App* subcom, mamba::Configuration& config);

void
set_repoquery_command(CLI::App* subcom, mamba::Configuration& config);

void
set_env_command(CLI::App* subcom, mamba::Configuration& config);

void
set_activate_command(CLI::App* subcom);

void
set_run_command(CLI::App* subcom, mamba::Configuration& config);

void
set_ps_command(CLI::App* subcom, mamba::Context& context);

void
get_completions(CLI::App* app, mamba::Configuration& config, int argc, char** argv);

void
set_search_command(CLI::App* subcom, mamba::Configuration& config);

void
set_auth_command(CLI::App* subcom);

#if !defined(_WIN32) && defined(MICROMAMBA_SERVER)
void
set_server_command(CLI::App* subcom, mamba::Configuration& config);
#endif

#endif
