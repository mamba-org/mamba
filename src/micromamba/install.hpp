#ifndef UMAMBA_INSTALL_HPP
#define UMAMBA_INSTALL_HPP

#ifdef VENDORED_CLI11
#include "mamba/CLI.hpp"
#else
#include <CLI/CLI.hpp>
#endif


void
init_install_parser(CLI::App* subcom);

void
set_install_command(CLI::App* subcom);

#endif
