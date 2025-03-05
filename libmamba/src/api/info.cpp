// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/env.hpp"
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
        struct InfoOptions
        {
            bool print_licenses = false;
            bool base = false;
            bool environments = false;
        };

        // Prints a sequence of string/json-value pairs in a pretty table.
        // requirements:
        // - T must be a sequence of pair-like elements;
        // - the elements of T must be composed of a `std::string` and a `nlhomann::json` objects
        template <typename T>
        void info_pretty_print(const T& items, const Context::OutputParams& params)
        {
            if (params.json)
            {
                return;
            }

            std::size_t key_max_length = 0;
            for (const auto& [key, value] : items)
            {
                std::size_t key_length = key.size();
                key_max_length = std::max(key_length, key_max_length);
            }
            ++key_max_length;

            auto stream = Console::stream();

            for (const auto& [key, value] : items)
            {
                auto blk_size = key_max_length - key.size();

                stream << "\n" << std::string(blk_size, ' ') << key << " : ";
                for (auto v = value.begin(); v != value.end(); ++v)
                {
                    stream << (*v).template get<std::string>();
                    if (v != (value.end() - 1))
                    {
                        stream << "\n" << std::string(key_max_length + 3, ' ');
                    }
                }
            }
        }

        // Prints a sequence of string/json-value pairs in a json format.
        // requirements:
        // - T must be a sequence of pair-like elements;
        // - the elements of T must be composed of a `std::string` and a `nlhomann::json` objects
        template <typename T>
        void info_json_print(const T& items)
        {
            std::map<std::string, nlohmann::json> items_map;
            for (const auto& [key, val] : items)
            {
                items_map.insert({ key, val });
            }

            Console::instance().json_write(items_map);
        }

        void
        print_info(Context& ctx, ChannelContext& channel_context, Configuration& config, InfoOptions options)
        {
            assert(&ctx == &config.context());

            using info_sequence = std::vector<std::tuple<std::string, nlohmann::json>>;

            if (options.print_licenses)
            {
                static const std::vector<std::pair<std::string, nlohmann::json>> licenses = {
                    { "micromamba",
                      "BSD license, Copyright 2019 QuantStack and the Mamba contributors." },
                    { "c_ares",
                      "MIT license, Copyright (c) 2007 - 2018, Daniel Stenberg with many contributors, see AUTHORS file." },
                    { "cli11",
                      "BSD license, CLI11 1.8 Copyright (c) 2017-2019 University of Cincinnati, developed by Henry Schreiner under NSF AWARD 1414736. All rights reserved." },
                    { "cpp_filesystem",
                      "MIT license, Copyright (c) 2018, Steffen Schümann <s.schuemann@pobox.com>" },
                    { "curl",
                      "MIT license, Copyright (c) 1996 - 2020, Daniel Stenberg, daniel@haxx.se, and many contributors, see the THANKS file." },
                    { "krb5",
                      "MIT license, Copyright 1985-2020 by the Massachusetts Institute of Technology." },
                    { "libarchive",
                      "New BSD license, The libarchive distribution as a whole is Copyright by Tim Kientzle and is subject to the copyright notice reproduced at the bottom of this file." },
                    { "libev",
                      "BSD license, All files in libev are Copyright (c)2007,2008,2009,2010,2011,2012,2013 Marc Alexander Lehmann." },
                    { "liblz4", "LZ4 Library, Copyright (c) 2011-2016, Yann Collet" },
                    { "libnghttp2",
                      "MIT license, Copyright (c) 2012, 2014, 2015, 2016 Tatsuhiro Tsujikawa; 2012, 2014, 2015, 2016 nghttp2 contributors" },
                    { "libopenssl_3", "Apache license, Version 2.0, January 2004" },
                    { "libopenssl",
                      "Apache license, Copyright (c) 1998-2019 The OpenSSL Project, All rights reserved; 1995-1998 Eric Young (eay@cryptsoft.com)" },
                    { "libsolv", "BSD license, Copyright (c) 2019, SUSE LLC" },
                    { "nlohmann_json", "MIT license, Copyright (c) 2013-2020 Niels Lohmann" },
                    { "reproc", "MIT license, Copyright (c) Daan De Meyer" },
                    { "fmt", "MIT license, Copyright (c) 2012-present, Victor Zverovich." },
                    { "spdlog", "MIT license, Copyright (c) 2016 Gabi Melman." },
                    { "zstd",
                      "BSD license, Copyright (c) 2016-present, Facebook, Inc. All rights reserved." },
                };
                info_json_print(licenses);
                info_pretty_print(licenses, ctx.output_params);
                return;
            }
            if (options.base)
            {
                info_sequence items{ { "base environment", ctx.prefix_params.root_prefix.string() } };

                info_json_print(items);
                info_pretty_print(items, ctx.output_params);
                return;
            }
            if (options.environments)
            {
                mamba::detail::print_envs_impl(config);
                return;
            }

            info_sequence items{ { "libmamba version", version() } };

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

        detail::InfoOptions options;
        options.print_licenses = config.at("print_licenses").value<bool>();
        options.base = config.at("base").value<bool>();
        options.environments = config.at("environments").value<bool>();

        auto channel_context = ChannelContext::make_conda_compatible(config.context());
        detail::print_info(config.context(), channel_context, config, std::move(options));

        config.operation_teardown();
    }
}  // mamba
