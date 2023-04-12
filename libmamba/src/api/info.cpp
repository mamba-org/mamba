// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/info.hpp"
#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/virtual_packages.hpp"


extern "C"
{
#include <archive.h>
#include <curl/curl.h>
}

namespace mamba
{
    void info()
    {
        auto& config = Configuration::instance();

        config.at("use_target_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(
                MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX | MAMBA_ALLOW_NOT_ENV_PREFIX
            );
        config.load();

        detail::print_info();

        config.operation_teardown();
    }

    std::string banner()
    {
        auto& ctx = Context::instance();
        return ctx.command_params.custom_banner.empty() ? mamba_banner
                                                        : ctx.command_params.custom_banner;
    }

    namespace detail
    {
        void info_pretty_print(std::vector<std::tuple<std::string, nlohmann::json>> items)
        {
            if (Context::instance().output_params.json)
            {
                return;
            }

            std::size_t key_max_length = 0;
            for (auto& item : items)
            {
                std::size_t key_length = std::get<0>(item).size();
                key_max_length = std::max(key_length, key_max_length);
            }
            ++key_max_length;

            auto stream = Console::stream();

            for (auto& item : items)
            {
                auto key = std::get<0>(item);
                auto val = std::get<1>(item);
                auto blk_size = key_max_length - std::get<0>(item).size();

                stream << "\n" << std::string(blk_size, ' ') << key << " : ";
                for (auto v = val.begin(); v != val.end(); ++v)
                {
                    stream << (*v).get<std::string>();
                    if (v != (val.end() - 1))
                    {
                        stream << "\n" << std::string(key_max_length + 3, ' ');
                    }
                }
            }
        }

        void info_json_print(std::vector<std::tuple<std::string, nlohmann::json>> items)
        {
            std::map<std::string, nlohmann::json> items_map;
            for (auto& [key, val] : items)
            {
                items_map.insert({ key, val });
            }

            Console::instance().json_write(items_map);
        }

        void print_info()
        {
            auto& ctx = Context::instance();
            std::vector<std::tuple<std::string, nlohmann::json>> items;

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

            items.push_back({ "environment", name });
            items.push_back({ "env location", location });

            // items.insert( { "shell level", { 1 } });
            items.push_back({ "user config files",
                              { (env::home_directory() / ".mambarc").string() } });

            Configuration& config = Configuration::instance();
            std::vector<std::string> sources;
            for (auto s : config.valid_sources())
            {
                sources.push_back(s.string());
            };
            items.push_back({ "populated config files", sources });

            items.push_back({ "libmamba version", version() });

            if (ctx.command_params.is_micromamba && !ctx.command_params.caller_version.empty())
            {
                items.push_back({ "micromamba version", ctx.command_params.caller_version });
            }

            items.push_back({ "curl version", curl_version() });
            items.push_back({ "libarchive version", archive_version_details() });

            std::vector<std::string> virtual_pkgs;
            for (auto pkg : get_virtual_packages())
            {
                virtual_pkgs.push_back(concat(pkg.name, "=", pkg.version, "=", pkg.build_string));
            }
            items.push_back({ "virtual packages", virtual_pkgs });

            std::vector<std::string> channels = ctx.channels;
            // Always append context channels
            auto& ctx_channels = Context::instance().channels;
            std::copy(ctx_channels.begin(), ctx_channels.end(), std::back_inserter(channels));
            std::vector<std::string> channel_urls;
            for (auto channel : get_channels(channels))
            {
                for (auto url : channel->urls(true))
                {
                    channel_urls.push_back(url);
                }
            }
            items.push_back({ "channels", channel_urls });

            items.push_back({ "base environment", ctx.root_prefix.string() });

            items.push_back({ "platform", ctx.platform });

            info_json_print(items);
            info_pretty_print(items);
        }
    }  // detail
}  // mamba
