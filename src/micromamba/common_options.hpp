// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_COMMON_OPTIONS_HPP
#define UMAMBA_COMMON_OPTIONS_HPP

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
init_general_options(CLI::App* subcom);

void
init_prefix_options(CLI::App* subcom);

void
target_prefix_hook(fs::path& prefix);

void
init_network_parser(CLI::App* subcom);

void
strict_channel_priority_hook(bool& value);

void
no_channel_priority_hook(bool& value);

void
init_channel_parser(CLI::App* subcom);

void
load_channel_options(mamba::Context& ctx);

void
channels_hook(std::vector<std::string>& channels);

void
override_channels_hook(bool& override_channels);

void
check_target_prefix(bool allow_root_prefix,
                    bool allow_fallback,
                    bool check_existing,
                    bool allow_existing);

void
root_prefix_hook(fs::path& prefix);

void
load_configuration(bool show_banner = true);

#endif
