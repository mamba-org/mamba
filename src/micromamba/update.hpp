// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_UPDATE_HPP
#define UMAMBA_UPDATE_HPP

#ifdef VENDORED_CLI11
#include "mamba/CLI.hpp"
#else
#include <CLI/CLI.hpp>
#endif


void
init_update_parser(CLI::App* subcom);

void
set_update_command(CLI::App* subcom);


#endif
