// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_SHELL_HPP
#define UMAMBA_SHELL_HPP

#ifdef VENDORED_CLI11
#include "mamba/CLI.hpp"
#else
#include <CLI/CLI.hpp>
#endif


void
init_shell_parser(CLI::App* subcom);

void
set_shell_command(CLI::App* subcom);

#endif
