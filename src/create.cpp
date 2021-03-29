// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/configuration.hpp"
#include "mamba/install.hpp"


namespace mamba
{
    void create(const std::vector<std::string>& specs, const fs::path& prefix)
    {
        auto& config = Configuration::instance();

        if (!prefix.empty())
            config.at("target_prefix").set_value(prefix);

        config.load(!MAMBA_ALLOW_ROOT_PREFIX & !MAMBA_ALLOW_FALLBACK_PREFIX
                    & !MAMBA_ALLOW_EXISTING_PREFIX);

        std::vector<std::string> create_specs = specs;
        if (specs.empty())
            create_specs = config.at("specs").value<std::vector<std::string>>();

        if (!create_specs.empty())
        {
            detail::install_specs(create_specs, true);
        }
        else
        {
            Console::print("Nothing to do.");
        }
    }
}
