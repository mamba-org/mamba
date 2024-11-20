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
#include "mamba/util/string.hpp"

namespace mamba
{


    namespace detail
    {
        struct list_options
        {
            bool full_name;
        };

        struct formatted_pkg
        {
            std::string name, version, build, channel;
        };

        bool compare_alphabetically(const formatted_pkg& a, const formatted_pkg& b)
        {
            return a.name < b.name;
        }

        std::string strip_from_filename_and_platform(
            const std::string& full_str,
            const std::string& filename,
            const std::string& platform
        )
        {
            using util::remove_suffix;
            return std::string(remove_suffix(
                remove_suffix(remove_suffix(remove_suffix(full_str, filename), "/"), platform),
                "/"
            ));
        }

        void list_packages(
            const Context& ctx,
            std::string regex,
            ChannelContext& channel_context,
            list_options options
        )
        {
            auto sprefix_data = PrefixData::create(ctx.prefix_params.target_prefix, channel_context);
            if (!sprefix_data)
            {
                // TODO: propagate tl::expected mechanism
                throw std::runtime_error(
                    fmt::format("could not load prefix data: {}", sprefix_data.error().what())
                );
            }
            PrefixData& prefix_data = sprefix_data.value();

            if (options.full_name)
            {
                regex = '^' + regex + '$';
            }

            std::regex spec_pat(regex);
            auto all_records = prefix_data.all_pkg_mgr_records();

            if (ctx.output_params.json)
            {
                auto jout = nlohmann::json::array();
                std::vector<std::string> keys;

                for (const auto& pkg : all_records)
                {
                    keys.push_back(pkg.first);
                }
                std::sort(keys.begin(), keys.end());

                for (const auto& key : keys)
                {
                    auto obj = nlohmann::json();
                    const auto& pkg_info = all_records.find(key)->second;

                    if (regex.empty() || std::regex_search(pkg_info.name, spec_pat))
                    {
                        auto channels = channel_context.make_channel(pkg_info.package_url);
                        assert(channels.size() == 1);  // A URL can only resolve to one channel

                        if (pkg_info.package_url.empty() && (pkg_info.channel == "pypi"))
                        {
                            obj["base_url"] = "https://pypi.org/";
                            obj["channel"] = pkg_info.channel;
                        }
                        else
                        {
                            obj["base_url"] = strip_from_filename_and_platform(
                                channels.front().url().str(specs::CondaURL::Credentials::Remove),
                                pkg_info.filename,
                                pkg_info.platform
                            );
                            obj["channel"] = strip_from_filename_and_platform(
                                channels.front().display_name(),
                                pkg_info.filename,
                                pkg_info.platform
                            );
                        }
                        obj["build_number"] = pkg_info.build_number;
                        obj["build_string"] = pkg_info.build_string;
                        obj["dist_name"] = pkg_info.str();
                        obj["name"] = pkg_info.name;
                        obj["platform"] = pkg_info.platform;
                        obj["version"] = pkg_info.version;
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
            for (const auto& package : all_records)
            {
                if (regex.empty() || std::regex_search(package.second.name, spec_pat))
                {
                    formatted_pkgs.name = package.second.name;
                    formatted_pkgs.version = package.second.version;
                    formatted_pkgs.build = package.second.build_string;
                    if (package.second.channel.find("https://repo.anaconda.com/pkgs/") == 0)
                    {
                        formatted_pkgs.channel = "";
                    }
                    else if (package.second.channel == "pypi")
                    {
                        formatted_pkgs.channel = package.second.channel;
                    }
                    else
                    {
                        auto channels = channel_context.make_channel(package.second.channel);
                        assert(channels.size() == 1);  // A URL can only resolve to one channel
                        formatted_pkgs.channel = strip_from_filename_and_platform(
                            channels.front().display_name(),
                            package.second.filename,
                            package.second.platform
                        );
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

    void list(Configuration& config, const std::string& regex)
    {
        config.at("use_target_prefix_fallback").set_value(true);
        config.at("use_default_prefix_fallback").set_value(true);
        config.at("use_root_prefix_fallback").set_value(true);
        config.at("target_prefix_checks")
            .set_value(
                MAMBA_ALLOW_EXISTING_PREFIX | MAMBA_ALLOW_MISSING_PREFIX
                | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX | MAMBA_EXPECT_EXISTING_PREFIX
            );
        config.load();

        detail::list_options options;
        options.full_name = config.at("full_name").value<bool>();
        auto channel_context = ChannelContext::make_conda_compatible(config.context());
        detail::list_packages(config.context(), regex, channel_context, std::move(options));
    }
}
