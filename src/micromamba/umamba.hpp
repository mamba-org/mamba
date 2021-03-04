// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_UMAMBA_HPP
#define UMAMBA_UMAMBA_HPP

#ifdef VENDORED_CLI11
#include "mamba/CLI.hpp"
#else
#include <CLI/CLI.hpp>
#endif


void
init_umamba_parser(CLI::App* com);

void
set_umamba_command(CLI::App* com);

#endif
