// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_ENV_HPP
#define MAMBA_API_ENV_HPP

#include "mamba/api/configuration.hpp"

namespace mamba
{
    class ChannelContext;
    class Configuration;
    class Context;

    void print_envs(Configuration& config);

    namespace detail
    {
        std::string get_env_name(const Context& ctx, const mamba::fs::u8path& px);
        void print_envs_impl(const Configuration& config);
    }
}

#endif
