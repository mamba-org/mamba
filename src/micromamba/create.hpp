// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_CREATE_HPP
#define UMAMBA_CREATE_HPP

#ifdef VENDORED_CLI11
#include "mamba/core/CLI.hpp"
#else
#include <CLI/CLI.hpp>
#endif


void
set_create_command(CLI::App* subcom);

#endif
