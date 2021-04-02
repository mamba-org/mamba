// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"


namespace mamba
{
    void create()
    {
        auto& config = Configuration::instance();

        int target_prefix_checks = MAMBA_NOT_ALLOW_ROOT_PREFIX | MAMBA_NOT_ALLOW_EXISTING_PREFIX
                                   | MAMBA_NOT_ALLOW_FALLBACK_PREFIX
                                   | MAMBA_NOT_ALLOW_MISSING_PREFIX | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX
                                   | MAMBA_NOT_EXPECT_EXISTING_PREFIX;

        config.load(target_prefix_checks);

        auto& create_specs = config.at("specs").value<std::vector<std::string>>();
        auto& use_explicit = config.at("explicit_install").value<bool>();

        if (use_explicit)
        {
            install_explicit_specs(create_specs);
        }
        else
        {
            install_specs(create_specs, true);
        }
    }
}
