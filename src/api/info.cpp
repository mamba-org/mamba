// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/info.hpp"
#include "mamba/api/install.hpp"

#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/version.hpp"
#include "mamba/core/virtual_packages.hpp"


namespace mamba
{
    void info()
    {
        auto& config = Configuration::instance();

        config.at("use_target_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                       | MAMBA_ALLOW_NOT_ENV_PREFIX);
        config.load();

        detail::print_info();

        config.operation_teardown();
    }

    std::string version()
    {
        return mamba_version;
    }

    std::string banner()
    {
        auto& ctx = Context::instance();
        return ctx.custom_banner.empty() ? mamba_banner : ctx.custom_banner;
    }

    namespace detail
    {
        void print_info()
        {
            auto& ctx = Context::instance();
            std::vector<std::tuple<std::string, std::vector<std::string>>> items;

            std::string name, location;
            if (!ctx.target_prefix.empty())
            {
                name = env_name(ctx.target_prefix);
                location = ctx.target_prefix.string();
            }
            else
            {
                name = "None";
                location = "-";
            }

            if (std::getenv("CONDA_PREFIX") && (std::getenv("CONDA_PREFIX") == ctx.target_prefix))
            {
                name += " (active)";
            }
            else if (fs::exists(ctx.target_prefix))
            {
                if (!(fs::exists(ctx.target_prefix / "conda-meta")
                      || (ctx.target_prefix == ctx.root_prefix)))
                {
                    name += " (not env)";
                }
            }
            else
            {
                name += " (not found)";
            }

            items.push_back({ "environment", { name } });
            items.push_back({ "env location", { location } });

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
        }

        void info_pretty_print(std::vector<std::tuple<std::string, std::vector<std::string>>> map)
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
    }  // detail
}  // mamba
