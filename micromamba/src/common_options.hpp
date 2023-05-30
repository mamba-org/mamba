// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_COMMON_OPTIONS_HPP
#define UMAMBA_COMMON_OPTIONS_HPP

#include <CLI/CLI.hpp>

#include "mamba/api/configuration.hpp"
#include "mamba/core/context.hpp"


void
init_rc_options(CLI::App* subcom, mamba::Configuration& config);

void
init_general_options(CLI::App* subcom, mamba::Configuration& config);

void
init_prefix_options(CLI::App* subcom, mamba::Configuration& config);

void
init_install_options(CLI::App* subcom, mamba::Configuration& config);

void
init_network_options(CLI::App* subcom, mamba::Configuration& config);

void
strict_channel_priority_hook(mamba::Configuration& config, bool& value);

void
no_channel_priority_hook(mamba::Configuration& config, bool& value);

void
init_channel_parser(CLI::App* subcom, mamba::Configuration& config);

void
load_channel_options(mamba::Context& ctx);

void
channels_hook(std::vector<std::string>& channels);

void
override_channels_hook(mamba::Configuration& config, bool& override_channels);

#endif
