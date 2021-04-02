// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/create.hpp"
#include "mamba/api/install.hpp"

#include "mamba/core/output.hpp"


namespace mamba
{
    void install()
    {
        auto& config = Configuration::instance();

        config.load(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                    | MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_NOT_ALLOW_MISSING_PREFIX
                    | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_EXPECT_EXISTING_PREFIX);

        auto& install_specs = config.at("specs").value<std::vector<std::string>>();
        auto& use_explicit = config.at("explicit_install").value<bool>();

        if (!install_specs.empty())
        {
            if (use_explicit)
            {
                install_explicit_specs(install_specs);
            }
            else
            {
                mamba::install_specs(install_specs, false);
            }
        }
        else
        {
            Console::print("Nothing to do.");
        }
    }
}  // mamba
