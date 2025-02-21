// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_LIST_HPP
#define MAMBA_API_LIST_HPP

#include <string>

#include <CLI/CLI.hpp>

#include "mamba/api/configuration.hpp"

namespace mamba
{
    class ChannelContext;
    class Configuration;
    class Context;
    
    namespace details
    {
        void set_env_list_subcommand(CLI::App* com, Configuration& config, std::string flag, std::string description);
    }
}

#endif