// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#include "mamba/api/configuration.hpp"
#include "mamba/api/info.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"


extern "C"
{
#include <archive.h>
#include <curl/curl.h>
}

namespace mamba
{
    namespace detail
    {
        struct list_options
        {
            bool base = false;
        };

        void info_pretty_print(
            std::vector<std::tuple<std::string, nlohmann::json>> items,
            const Context::OutputParams& params
        )
        {
            if (params.json)
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

        void print_info(
            Context& ctx,
            ChannelContext& channel_context,
            const Configuration& config,
            list_options options
        )
        {
            assert(&ctx == &config.context());

            if (options.base)
            {
                std::cout << ctx.prefix_params.root_prefix.string() << std::endl;
                return;
            }

            std::vector<std::tuple<std::string, nlohmann::json>> items;

            items.push_back({ "libmamba version", version() });

            if (ctx.command_params.is_mamba_exe && !ctx.command_params.caller_version.empty())
            {
                items.push_back({
                    fmt::format("{} version", get_self_exe_path().stem().string()),
                    ctx.command_params.caller_version,
                });
            }

            items.push_back({ "curl version", curl_version() });
            items.push_back({ "libarchive version", archive_version_details() });

            items.push_back({ "envs directories", ctx.envs_dirs });
            items.push_back({ "package cache", ctx.pkgs_dirs });

            std::string name, location;
            if (!ctx.prefix_params.target_prefix.empty())
            {
                name = env_name(ctx);
                location = ctx.prefix_params.target_prefix.string();
            }
            else
            {
                name = "None";
                location = "-";
            }

            if (auto prefix = util::get_env("CONDA_PREFIX"); prefix == ctx.prefix_params.target_prefix)
            {
                name += " (active)";
            }
            else if (fs::exists(ctx.prefix_params.target_prefix))
            {
                if (!(fs::exists(ctx.prefix_params.target_prefix / "conda-meta")
                      || (ctx.prefix_params.target_prefix == ctx.prefix_params.root_prefix)))
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
            items.push_back({
                "user config files",
                { util::path_concat(util::user_home_dir(), ".mambarc") },
            });

            std::vector<std::string> sources;
            for (auto s : config.valid_sources())
            {
                sources.push_back(s.string());
            };
            items.push_back({ "populated config files", sources });

            std::vector<std::string> virtual_pkgs;
            for (auto pkg : get_virtual_packages(ctx.platform))
            {
                virtual_pkgs.push_back(util::concat(pkg.name, "=", pkg.version, "=", pkg.build_string)
                );
            }
            items.push_back({ "virtual packages", virtual_pkgs });

            // Always append context channels
            std::vector<std::string> channel_urls;
            using Credentials = specs::CondaURL::Credentials;
            channel_urls.reserve(ctx.channels.size() * 2);  // Lower bound * (platform + noarch)
            for (const auto& loc : ctx.channels)
            {
                for (auto channel : channel_context.make_channel(loc))
                {
                    for (auto url : channel.platform_urls())
                    {
                        channel_urls.push_back(std::move(url).str(Credentials::Remove));
                    }
                }
            }
            items.push_back({ "channels", channel_urls });

            items.push_back({ "base environment", ctx.prefix_params.root_prefix.string() });

            items.push_back({ "platform", ctx.platform });

            info_json_print(items);
            info_pretty_print(items, ctx.output_params);
        }
    }  // detail

    void info(Configuration& config)
    {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("use_default_prefix_fallback").set_value(true);
        config.at("use_root_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(
                MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX | MAMBA_ALLOW_NOT_ENV_PREFIX
            );
        config.load();

        detail::list_options options;
        options.base = config.at("base").value<bool>();

        auto channel_context = ChannelContext::make_conda_compatible(config.context());
        detail::print_info(config.context(), channel_context, config, std::move(options));

        config.operation_teardown();
    }
}  // mamba
