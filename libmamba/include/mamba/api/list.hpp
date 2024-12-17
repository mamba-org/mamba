// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_LIST_HPP
#define MAMBA_API_LIST_HPP

#include <string>

namespace mamba
{
    class ChannelContext;
    class Configuration;
    class Context;

    void list(Configuration& config, const std::string& regex);
}

#endif
