#ifndef UMAMBA_LIST_HPP
#define UMAMBA_LIST_HPP

#include "mamba/context.hpp"

#ifdef VENDORED_CLI11
#include "mamba/CLI.hpp"
#else
#include <CLI/CLI.hpp>
#endif

#include <string>


void
init_list_parser(CLI::App* subcom);

void
set_list_command(CLI::App* subcom);

#endif
