// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_CLEAN_HPP
#define UMAMBA_CLEAN_HPP

#include "mamba/context.hpp"

#ifdef VENDORED_CLI11
#include "mamba/CLI.hpp"
#else
#include <CLI/CLI.hpp>
#endif


void
init_clean_parser(CLI::App* subcom);

void
set_clean_command(CLI::App* subcom);

#endif
