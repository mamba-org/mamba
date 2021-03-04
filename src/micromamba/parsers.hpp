// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_PARSERS_HPP
#define UMAMBA_PARSERS_HPP

#include "options.hpp"

#include "mamba/context.hpp"

#ifdef VENDORED_CLI11
#include "mamba/CLI.hpp"
#else
#include <CLI/CLI.hpp>
#endif


#define SET_BOOLEAN_FLAG(NAME)                                                                     \
    ctx.NAME = create_options.NAME ? ((create_options.NAME == 1) ? true : false) : ctx.NAME;


void
init_rc_options(CLI::App* subcom);

void
load_rc_options(mamba::Context& ctx);

void
init_general_options(CLI::App* subcom);

void
load_general_options(mamba::Context& ctx);

void
init_prefix_options(CLI::App* subcom);

void
load_prefix_options(mamba::Context& ctx);

void
init_network_parser(CLI::App* subcom);

void
load_network_options(mamba::Context& ctx);

void
init_channel_parser(CLI::App* subcom);

void
load_channel_options(mamba::Context& ctx);

void
catch_existing_target_prefix(mamba::Context& ctx);

void
check_root_prefix(bool silent = false);

#endif
