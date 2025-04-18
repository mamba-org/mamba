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
        struct ListOptions
        {
            bool full_name = false;
            bool no_pip = false;
            bool reverse = false;
            bool explicit_ = false;
            bool md5 = false;
            bool canonical = false;
            bool export_ = false;
            bool revisions = false;
            enum class Format
            {
                JSON,
                TABLE
            } format
                = Format::TABLE;
        };

        struct formatted_pkg
        {
            std::string name, version, build, channel, url, md5, build_string, platform;

            void print(std::ostream& out) const
            {
                out << name << " " << version << " " << build << " " << channel << std::endl;
            }
        };

        bool compare_alphabetically(const formatted_pkg& a, const formatted_pkg& b)
        {
            return a.name < b.name;
        }

        bool compare_reverse_alphabetically(const formatted_pkg& a, const formatted_pkg& b)
        {
            return a.name >= b.name;
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

        std::vector<std::string>
        get_record_keys(ListOptions options, const PrefixData::package_map& all_records)
        {
            std::vector<std::string> keys;

            for (const auto& pkg : all_records)
            {
                keys.push_back(pkg.first);
            }
            if (options.reverse)
            {
                std::sort(
                    keys.begin(),
                    keys.end(),
                    [](const std::string& a, const std::string& b) { return a >= b; }
                );
            }
            else
            {
                std::sort(keys.begin(), keys.end());
            }

            return keys;
        }

        std::string
        get_formatted_channel(const specs::PackageInfo& pkg_info, const specs::Channel& channel)
        {
            if (pkg_info.channel == "pypi")
            {
                return "pypi";
            }
            else
            {
                return strip_from_filename_and_platform(
                    channel.display_name(),
                    pkg_info.filename,
                    pkg_info.platform
                );
            }
        }

        std::string get_base_url(const specs::PackageInfo& pkg_info, const specs::Channel& channel)
        {
            if (pkg_info.channel == "pypi")
            {
                return "https://pypi.org/";
            }
            else
            {
                return strip_from_filename_and_platform(
                    channel.url().str(specs::CondaURL::Credentials::Remove),
                    pkg_info.filename,
                    pkg_info.platform
                );
            }
        }

        void list_packages(
            const Context& ctx,
            std::string regex,
            ChannelContext& channel_context,
            ListOptions options
        )
        {
            auto sprefix_data = PrefixData::create(
                ctx.prefix_params.target_prefix,
                channel_context,
                options.no_pip
            );
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

            auto accept_package = [&regex, &spec_pat, &options](const specs::PackageInfo& pkg_info
                                  ) -> bool
            {
                if (regex.empty())
                {
                    return true;
                }
                if (options.full_name)
                {
                    return std::regex_match(pkg_info.name, spec_pat);
                }
                return std::regex_search(pkg_info.name, spec_pat);
            };

            if (options.format == ListOptions::Format::JSON)
            {
                nlohmann::json j;
                for (const auto& [name, version_map] : prefix_data.records())
                {
                    for (const auto& [version, builds] : version_map)
                    {
                        for (const auto& pkg_info : builds)
                        {
                            if (accept_package(pkg_info))
                            {
                                nlohmann::json obj;
                                auto channels = channel_context.make_channel(pkg_info.channel);
                                assert(channels.size() == 1);  // A URL can only resolve to one
                                                               // channel

                                obj["channel"] = get_formatted_channel(pkg_info, channels.front());
                                obj["url"] = pkg_info.package_url;
                                obj["md5"] = pkg_info.md5;
                                obj["build_number"] = pkg_info.build_number;
                                obj["build_string"] = pkg_info.build_string;
                                obj["dist_name"] = pkg_info.str();
                                obj["name"] = pkg_info.name;
                                obj["platform"] = pkg_info.platform;
                                obj["version"] = pkg_info.version;
                                j.push_back(obj);
                            }
                        }
                    }
                }
                std::cout << j.dump(4) << std::endl;
            }
            else
            {
                std::cout << "List of packages in environment: " << ctx.prefix_params.target_prefix
                          << "\n\n";

                formatted_pkg formatted_pkgs;

                std::vector<formatted_pkg> packages;

                for (const auto& [name, version_map] : prefix_data.records())
                {
                    for (const auto& [version, builds] : version_map)
                    {
                        for (const auto& pkg_info : builds)
                        {
                            if (accept_package(pkg_info))
                            {
                                auto channels = channel_context.make_channel(pkg_info.channel);
                                assert(channels.size() == 1);  // A URL can only resolve to one
                                                               // channel

                                formatted_pkgs.channel = get_formatted_channel(
                                    pkg_info,
                                    channels.front()
                                );
                                formatted_pkgs.name = pkg_info.name;
                                formatted_pkgs.version = pkg_info.version;
                                formatted_pkgs.build = pkg_info.build_string;
                                formatted_pkgs.url = pkg_info.package_url;
                                formatted_pkgs.md5 = pkg_info.md5;
                                formatted_pkgs.build_string = pkg_info.build_string;
                                formatted_pkgs.platform = pkg_info.platform;
                                packages.push_back(formatted_pkgs);
                            }
                        }
                    }
                }

                auto comparator = options.reverse ? compare_reverse_alphabetically
                                                  : compare_alphabetically;
                std::sort(packages.begin(), packages.end(), comparator);

                // format and print output
                if (options.revisions)
                {
                    if (options.explicit_)
                    {
                        LOG_WARNING
                            << "Option --explicit ignored because --revisions was also provided.";
                    }
                    if (options.canonical)
                    {
                        LOG_WARNING
                            << "Option --canonical ignored because --revisions was also provided.";
                    }
                    if (options.export_)
                    {
                        LOG_WARNING
                            << "Option --export ignored because --revisions was also provided.";
                    }
                    auto user_requests = prefix_data.history().get_user_requests();
                    for (auto r : user_requests)
                    {
                        if ((r.link_dists.size() > 0) || (r.unlink_dists.size() > 0))
                        {
                            std::cout << r.date << " (rev " << r.revision_num << ")" << std::endl;
                            for (auto ud : r.unlink_dists)
                            {
                                std::cout << "-" << ud << std::endl;
                            }
                            for (auto ld : r.link_dists)
                            {
                                std::cout << "+" << ld << std::endl;
                            }
                            std::cout << std::endl;
                        }
                    }
                }
                else if (options.explicit_)
                {
                    if (options.canonical)
                    {
                        LOG_WARNING
                            << "Option --canonical ignored because --explicit was also provided.";
                    }
                    if (options.export_)
                    {
                        LOG_WARNING
                            << "Option --export ignored because --explicit was also provided.";
                    }
                    for (auto p : packages)
                    {
                        if (options.md5)
                        {
                            std::cout << p.url << "#" << p.md5 << std::endl;
                        }
                        else
                        {
                            std::cout << p.url << std::endl;
                        }
                    }
                }
                else if (options.canonical)
                {
                    if (options.export_)
                    {
                        LOG_WARNING
                            << "Option --export ignored because --canonical was also provided.";
                    }
                    for (auto p : packages)
                    {
                        std::cout << p.channel << "/" << p.platform << "::" << p.name << "-"
                                  << p.version << "-" << p.build_string << std::endl;
                    }
                }
                else if (options.export_)
                {
                    for (auto p : packages)
                    {
                        std::cout << p.name << "=" << p.version << "=" << p.build_string << std::endl;
                    }
                }
                else
                {
                    auto requested_specs = prefix_data.history().get_requested_specs_map();
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

        detail::ListOptions options;
        options.full_name = config.at("full_name").value<bool>();
        options.no_pip = config.at("no_pip").value<bool>();
        options.reverse = config.at("reverse").value<bool>();
        options.explicit_ = config.at("explicit").value<bool>();
        options.md5 = config.at("md5").value<bool>();
        options.canonical = config.at("canonical").value<bool>();
        options.export_ = config.at("export").value<bool>();
        options.revisions = config.at("revisions").value<bool>();

        auto channel_context = ChannelContext::make_conda_compatible(config.context());
        detail::list_packages(config.context(), regex, channel_context, std::move(options));
    }
}
