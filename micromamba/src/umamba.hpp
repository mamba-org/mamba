// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_UMAMBA_HPP
#define UMAMBA_UMAMBA_HPP

#include <CLI/CLI.hpp>


const char banner[] = R"MAMBARAW(
                                           __
          __  ______ ___  ____ _____ ___  / /_  ____ _
         / / / / __ `__ \/ __ `/ __ `__ \/ __ \/ __ `/
        / /_/ / / / / / / /_/ / / / / / / /_/ / /_/ /
       / .___/_/ /_/ /_/\__,_/_/ /_/ /_/_.___/\__,_/
      /_/
)MAMBARAW";

void
set_clean_command(CLI::App* subcom);

void
set_config_command(CLI::App* subcom);

void
set_constructor_command(CLI::App* subcom);

void
set_create_command(CLI::App* subcom);

void
set_info_command(CLI::App* subcom);

void
set_install_command(CLI::App* subcom);

void
set_list_command(CLI::App* subcom);

void
set_remove_command(CLI::App* subcom);

void
set_shell_command(CLI::App* subcom);

void
set_package_command(CLI::App* subcom);

void
set_umamba_command(CLI::App* com);

void
set_update_command(CLI::App* subcom);

void
set_self_update_command(CLI::App* subcom);

void
set_repoquery_command(CLI::App* subcom);

void
set_env_command(CLI::App* subcom);

void
set_activate_command(CLI::App* subcom);

void
set_run_command(CLI::App* subcom);

void
set_ps_command(CLI::App* subcom);

void
get_completions(CLI::App* app, int argc, char** argv);

void
set_search_command(CLI::App* subcom);

void
set_auth_command(CLI::App* subcom);

#endif
