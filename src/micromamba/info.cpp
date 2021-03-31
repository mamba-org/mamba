// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "info.hpp"
#include "common_options.hpp"

#include "mamba/core/configuration.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/version.hpp"
#include "mamba/core/virtual_packages.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_info_parser(CLI::App* subcom)
{
    init_prefix_options(subcom);
}

void
set_info_command(CLI::App* subcom)
{
    init_info_parser(subcom);

    subcom->callback([&]() {
        load_configuration(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                           | MAMBA_ALLOW_EXISTING_PREFIX);

        auto& ctx = Context::instance();
        std::vector<std::tuple<std::string, std::vector<std::string>>> items;

        if (!ctx.target_prefix.empty())
        {
            auto split_prefix = rsplit(ctx.target_prefix.string(), "/", 1);
            items.push_back({ "active environment", { env_name(ctx.target_prefix) } });
            items.push_back({ "active env location", { ctx.target_prefix.string() } });
        }
        else
        {
            items.push_back({ "active environment", { "None" } });
        }
        // items.insert( { "shell level", { 1 } });
        items.push_back({ "user config files", { env::home_directory() / ".mambarc" } });

        Configuration& config = Configuration::instance();
        std::vector<std::string> sources;
        for (auto s : config.valid_sources())
        {
            sources.push_back(s.string());
        };
        items.push_back({ "populated config files", sources });

        items.push_back({ "micromamba version", { version() } });

        std::vector<std::string> virtual_pkgs;
        for (auto pkg : get_virtual_packages())
        {
            virtual_pkgs.push_back(concat(pkg.name, "=", pkg.version, "=", pkg.build_string));
        }
        items.push_back({ "virtual packages", virtual_pkgs });

        items.push_back({ "base environment", { ctx.root_prefix.string() } });

        items.push_back({ "platform", { ctx.platform() } });

        info_pretty_print(items);
    });
}

void
info_pretty_print(std::vector<std::tuple<std::string, std::vector<std::string>>> map)
{
    int key_max_length = 0;
    for (auto item : map)
    {
        int key_length = std::get<0>(item).size();
        key_max_length = key_length < key_max_length ? key_max_length : key_length;
    }
    ++key_max_length;

    std::cout << std::endl;
    for (auto item : map)
    {
        auto key = std::get<0>(item);
        auto val = std::get<1>(item);
        int blk_size = key_max_length - std::get<0>(item).size();

        std::cout << std::string(blk_size, ' ') << key << " : ";
        for (auto v = val.begin(); v != val.end(); ++v)
        {
            std::cout << *v;
            if (v != (val.end() - 1))
            {
                std::cout << std::endl << std::string(key_max_length + 3, ' ');
            }
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

std::string
version()
{
    return mamba_version;
}
