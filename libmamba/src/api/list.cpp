// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <regex>

#include "mamba/api/configuration.hpp"
#include "mamba/api/list.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/prefix_data.hpp"

namespace mamba
{
    void list(Configuration& config, const std::string& regex)
    {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(
                MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_EXPECT_EXISTING_PREFIX
            );
        config.load();

        auto channel_context = ChannelContext::make_conda_compatible(config.context());
        detail::list_packages(config.context(), regex, channel_context);
    }

    namespace detail
    {
        struct formatted_pkg
        {
            std::string name, version, build, channel;
        };

        bool compare_alphabetically(const formatted_pkg& a, const formatted_pkg& b)
        {
            return a.name < b.name;
        }

        void list_packages(const Context& ctx, std::string regex, ChannelContext& channel_context)
        {
            auto sprefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
            if (!sprefix_data)
            {
                // TODO: propagate tl::expected mechanism
                throw std::runtime_error("could not load prefix data");
            }
            PrefixData& prefix_data = sprefix_data.value();

            std::regex spec_pat(regex);

            if (ctx.output_params.json)
            {
                auto jout = nlohmann::json::array();
                std::vector<std::string> keys;

                for (const auto& pkg : prefix_data.records())
                {
                    keys.push_back(pkg.first);
                }
                std::sort(keys.begin(), keys.end());

                for (const auto& key : keys)
                {
                    auto obj = nlohmann::json();
                    const auto& pkg_info = prefix_data.records().find(key)->second;

                    if (regex.empty() || std::regex_search(pkg_info.name(), spec_pat))
                    {
                        auto channels = channel_context.make_channel(pkg_info.url);
                        assert(channels.size() == 1);  // A URL can only resolve to one channel
                        obj["base_url"] = channels.front().url().str(specs::CondaURL::Credentials::Remove
                        );
                        obj["build_number"] = pkg_info.build_number;
                        obj["build_string"] = pkg_info.build_string;
                        obj["channel"] = channels.front().display_name();
                        obj["dist_name"] = pkg_info.str();
                        obj["name"] = pkg_info.name();
                        obj["platform"] = pkg_info.subdir;
                        obj["version"] = pkg_info.version();
                        jout.push_back(obj);
                    }
                }
                std::cout << jout.dump(4) << std::endl;
                return;
            }

            std::cout << "List of packages in environment: " << ctx.prefix_params.target_prefix
                      << "\n\n";

            formatted_pkg formatted_pkgs;

            std::vector<formatted_pkg> packages;
            auto requested_specs = prefix_data.history().get_requested_specs_map();

            // order list of packages from prefix_data by alphabetical order
            for (const auto& package : prefix_data.records())
            {
                if (regex.empty() || std::regex_search(package.second.name(), spec_pat))
                {
                    formatted_pkgs.name = package.second.name();
                    formatted_pkgs.version = package.second.version();
                    formatted_pkgs.build = package.second.build_string;
                    if (package.second.channel.find("https://repo.anaconda.com/pkgs/") == 0)
                    {
                        formatted_pkgs.channel = "";
                    }
                    else
                    {
                        auto channels = channel_context.make_channel(package.second.url);
                        assert(channels.size() == 1);  // A URL can only resolve to one channel
                        formatted_pkgs.channel = channels.front().display_name();
                    }
                    packages.push_back(formatted_pkgs);
                }
            }

            std::sort(packages.begin(), packages.end(), compare_alphabetically);

            // format and print table
            printers::Table t({ "Name", "Version", "Build", "Channel" });
            t.set_alignment({ printers::alignment::left,
                              printers::alignment::left,
                              printers::alignment::left,
                              printers::alignment::left });
            t.set_padding({ 2, 2, 2, 2 });

            for (auto p : packages)
            {
                printers::FormattedString formatted_name(p.name);
                if (requested_specs.find(p.name) != requested_specs.end())
                {
                    formatted_name = printers::FormattedString(p.name);
                    formatted_name.style = ctx.graphics_params.palette.user;
                }
                t.add_row({ formatted_name, p.version, p.build, p.channel });
            }

            t.print(std::cout);
        }
    }
}
