// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/context.hpp"
#include "mamba/configuration.hpp"
#include "mamba/install.hpp"
#include "mamba/virtual_packages.hpp"


namespace mamba
{
    void update(std::vector<std::string>& specs, bool update_all, const fs::path& prefix)
    {
        auto& ctx = Context::instance();
        auto& config = Configuration::instance();

        if (!prefix.empty())
            config.at("target_prefix").set_value(prefix);

        config.load(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                    | MAMBA_ALLOW_EXISTING_PREFIX);

        if (update_all)
        {
            PrefixData prefix_data(ctx.target_prefix);
            prefix_data.load();

            for (const auto& package : prefix_data.m_package_records)
            {
                auto name = package.second.name;
                if (name != "python")
                {
                    specs.push_back(name);
                }
            }
        }

        if (!specs.empty())
        {
            detail::install_specs(specs, false, SOLVER_UPDATE);
        }
        else
        {
            Console::print("Nothing to do.");
        }
    }
}
