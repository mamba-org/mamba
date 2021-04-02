// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"


namespace mamba
{
    void config_list()
    {
        // load_configuration(false);

        auto& config = Configuration::instance();
        /*
                auto& show_sources = config.at("config_show_sources").value<bool>();
                auto& show_all = config.at("config_show_all").value<bool>();
                auto& show_groups = config.at("config_show_groups").value<bool>();
                auto& show_desc = config.at("config_show_descriptions").value<bool>();
                auto& show_long_desc = config.at("config_show_long_descriptions").value<bool>();
                auto& specs = config.at("config_specs").value<std::vector<std::string>>();
                std::cout << config.dump(
                    true, show_sources, show_all, show_groups, show_desc, show_long_desc, specs)
                            << std::endl;
        */
        std::cout << config.dump(true, true, false, false, false, false, {}) << std::endl;
    }
}
