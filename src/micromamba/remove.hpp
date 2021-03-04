// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_REMOVE_HPP
#define UMAMBA_REMOVE_HPP

#include "mamba/context.hpp"

#ifdef VENDORED_CLI11
#include "mamba/CLI.hpp"
#else
#include <CLI/CLI.hpp>
#endif

#include <string>
#include <vector>


void
init_remove_parser(CLI::App* subcom);

void
set_remove_command(CLI::App* subcom);

void
load_remove_options(mamba::Context& ctx);

void
remove_specs(const std::vector<std::string>& specs);

#endif
