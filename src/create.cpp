// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/install.hpp"


namespace mamba
{
    void create(const std::vector<std::string>& specs, const fs::path& prefix)
    {
        auto& ctx = Context::instance();
        if (!prefix.empty())
            ctx.target_prefix = prefix;

        using namespace detail;

        if (!check_target_prefix(0))
        {
            install_specs(specs, true);
        }
    }
}
