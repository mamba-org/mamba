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

        config.load(!MAMBA_ALLOW_ROOT_PREFIX & !MAMBA_ALLOW_FALLBACK_PREFIX
                    & !MAMBA_ALLOW_EXISTING_PREFIX);

        auto& create_specs = config.at("specs").value<std::vector<std::string>>();
        auto& use_explicit = config.at("explicit_install").value<bool>();

        if (use_explicit)
        {
            detail::install_explicit_specs(create_specs);
        }
        else
        {
            detail::install_specs(create_specs, true);
        }
    }
}
