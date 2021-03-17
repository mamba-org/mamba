#include "mamba/list.hpp"
#include "mamba/install.hpp"
#include "mamba/channel.hpp"
#include "mamba/configuration.hpp"
#include "mamba/prefix_data.hpp"


namespace mamba
{
    void list(const std::string& regex, const fs::path& prefix)
    {
        auto& ctx = Context::instance();

        if (!prefix.empty())
            ctx.target_prefix = prefix;

        using namespace detail;

        if (!check_target_prefix(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                                 | MAMBA_ALLOW_EXISTING_PREFIX))
            list_packages(regex);
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

        void list_packages(std::string regex)
        {
            auto& ctx = Context::instance();

            PrefixData prefix_data(ctx.target_prefix);
            prefix_data.load();

            std::regex spec_pat(regex);

            if (ctx.json)
            {
                auto jout = nlohmann::json::array();
                std::vector<std::string> keys;

                for (const auto& pkg : prefix_data.m_package_records)
                {
                    keys.push_back(pkg.first);
                }
                std::sort(keys.begin(), keys.end());

                for (const auto& key : keys)
                {
                    auto obj = nlohmann::json();
                    const auto& pkg_info = prefix_data.m_package_records.find(key)->second;

                    if (regex.empty() || std::regex_search(pkg_info.name, spec_pat))
                    {
                        auto channel = make_channel(pkg_info.url);
                        obj["base_url"] = channel.base_url();
                        obj["build_number"] = pkg_info.build_number;
                        obj["build_string"] = pkg_info.build_string;
                        obj["channel"] = channel.name();
                        obj["dist_name"] = pkg_info.str();
                        obj["name"] = pkg_info.name;
                        obj["platform"] = pkg_info.subdir;
                        obj["version"] = pkg_info.version;
                        jout.push_back(obj);
                    }
                }
                std::cout << jout.dump(4) << std::endl;
                return;
            }

            std::cout << "List of packages in environment: " << ctx.target_prefix << std::endl;

            formatted_pkg formatted_pkgs;

            std::vector<formatted_pkg> packages;

            // order list of packages from prefix_data by alphabetical order
            for (const auto& package : prefix_data.m_package_records)
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
                    else
                    {
                        Channel& channel = make_channel(package.second.url);
                        formatted_pkgs.channel = channel.name();
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
                t.add_row({ p.name, p.version, p.build, p.channel });
            }

            t.print(std::cout);
        }
    }
}
