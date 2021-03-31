// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_CONSTRUCTOR_HPP
#define UMAMBA_CONSTRUCTOR_HPP

#include "mamba/core/mamba_fs.hpp"

#ifdef VENDORED_CLI11
#include "mamba/core/CLI.hpp"
#else
#include <CLI/CLI.hpp>
#endif


void
init_constructor_parser(CLI::App* subcom);

void
set_constructor_command(CLI::App* subcom);

void
read_binary_from_stdin_and_write_to_file(fs::path& filename);

#endif
