// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <tuple>

#include "mamba/core/channel.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/util_string.hpp"

namespace mamba
{
    namespace
    {
        template <class T>
        std::string get_package_info_field(const PackageInfo&, T PackageInfo::*field);

        template <>
        std::string
        get_package_info_field<std::string>(const PackageInfo& pkg, std::string PackageInfo::*field)
        {
            return pkg.*field;
        }

        template <>
        std::string get_package_info_field<size_t>(const PackageInfo& pkg, size_t PackageInfo::*field)
        {
            return std::to_string(pkg.*field);
        }

        template <class T>
        PackageInfo::field_getter build_field_getter(T PackageInfo::*field)
        {
            return std::bind(get_package_info_field<T>, std::placeholders::_1, field);
        }

        using field_getter_map = std::map<std::string_view, PackageInfo::field_getter>;

        field_getter_map build_field_getter_map()
        {
            field_getter_map res;
            res["name"] = build_field_getter(&PackageInfo::name);
            res["version"] = build_field_getter(&PackageInfo::version);
            res["build_string"] = build_field_getter(&PackageInfo::build_string);
            res["build_number"] = build_field_getter(&PackageInfo::build_number);
            res["channel"] = build_field_getter(&PackageInfo::channel);
            res["url"] = build_field_getter(&PackageInfo::url);
            res["subdir"] = build_field_getter(&PackageInfo::subdir);
            res["fn"] = build_field_getter(&PackageInfo::fn);
            res["license"] = build_field_getter(&PackageInfo::license);
            res["size"] = build_field_getter(&PackageInfo::size);
            res["timestamp"] = build_field_getter(&PackageInfo::timestamp);
            return res;
        }

        const field_getter_map& get_field_getter_map()
        {
            static field_getter_map m = build_field_getter_map();
            return m;
        }
    }  // namespace

    PackageInfo::field_getter PackageInfo::get_field_getter(std::string_view field_name)
    {
        auto it = get_field_getter_map().find(field_name);
        if (it == get_field_getter_map().end())
        {
            throw std::runtime_error("field_getter function not found");
        }
        return it->second;
    }

    PackageInfo::compare_fun PackageInfo::less(std::string_view member)
    {
        auto getter = get_field_getter(member);
        return [getter](const PackageInfo& lhs, const PackageInfo& rhs)
        { return getter(lhs) < getter(rhs); };
    }

    PackageInfo::compare_fun PackageInfo::equal(std::string_view member)
    {
        auto getter = get_field_getter(member);
        return [getter](const PackageInfo& lhs, const PackageInfo& rhs)
        { return getter(lhs) == getter(rhs); };
    }

    PackageInfo::PackageInfo(nlohmann::json&& j)
    {
        using namespace std::string_literals;  // NOLINT(build/namespaces)
        assign_or(j, "name", name, ""s);
        assign_or(j, "version", version, ""s);
        assign_or(j, "channel", channel, ""s);
        assign_or(j, "url", url, ""s);
        assign_or(j, "subdir", subdir, ""s);
        assign_or(j, "fn", fn, ""s);
        assign_or(j, "size", size, size_t(0));
        assign_or(j, "timestamp", timestamp, size_t(0));
        std::string bs;
        assign_or(j, "build", bs, "<UNKNOWN>"s);
        if (bs == "<UNKNOWN>")
        {
            assign_or(j, "build_string", bs, ""s);
        }
        build_string = bs;
        assign_or(j, "build_number", build_number, size_t(0));
        assign_or(j, "license", license, ""s);
        assign_or(j, "md5", md5, ""s);
        assign_or(j, "sha256", sha256, ""s);
        if (j.contains("track_features"))
        {
            auto j_track_features = j["track_features"].get<std::string_view>();
            if (!j_track_features.empty())  // Split empty string would have an empty element
            {
                track_features = split(j_track_features, ",");
            }
        }

        // add the noarch type if we know it (only known for installed packages)
        if (j.contains("noarch"))
        {
            if (j["noarch"].type() == nlohmann::json::value_t::boolean)
            {
                noarch = "generic_v1";
            }
            else
            {
                assign_or(j, "noarch", noarch, ""s);
            }
        }

        if (j.contains("depends"))
        {
            depends = j["depends"].get<std::vector<std::string>>();
        }
        if (j.contains("constrains"))
        {
            constrains = j["constrains"].get<std::vector<std::string>>();
        }
    }

    PackageInfo::PackageInfo(std::string n)
        : name(std::move(n))
    {
    }

    PackageInfo::PackageInfo(std::string n, std::string v, std::string b, std::size_t bn)
        : name(std::move(n))
        , version(std::move(v))
        , build_string(std::move(b))
        , build_number(std::move(bn))
    {
    }

    bool PackageInfo::operator==(const PackageInfo& other) const
    {
        auto attrs = [](const PackageInfo& p)
        {
            return std::tie(
                p.name,
                p.version,
                p.build_string,
                p.noarch,
                p.build_number,
                p.channel,
                p.url,
                p.subdir,
                p.fn,
                p.license,
                p.size,
                p.timestamp,
                p.md5,
                p.sha256,
                p.track_features,
                p.depends,
                p.constrains,
                p.signatures,
                p.defaulted_keys
            );
        };
        return attrs(*this) == attrs(other);
    }

    nlohmann::json PackageInfo::json_record() const
    {
        nlohmann::json j;
        j["name"] = name;
        j["version"] = version;
        j["channel"] = channel;
        j["url"] = url;
        j["subdir"] = subdir;
        j["fn"] = fn;
        j["size"] = size;
        j["timestamp"] = timestamp;
        j["build"] = build_string;
        j["build_string"] = build_string;
        j["build_number"] = build_number;
        j["license"] = license;
        j["track_features"] = fmt::format("{}", fmt::join(track_features, ","));
        if (!md5.empty())
        {
            j["md5"] = md5;
        }
        if (!sha256.empty())
        {
            j["sha256"] = sha256;
        }
        if (depends.empty())
        {
            j["depends"] = nlohmann::json::array();
        }
        else
        {
            j["depends"] = depends;
        }

        if (constrains.empty())
        {
            j["constrains"] = nlohmann::json::array();
        }
        else
        {
            j["constrains"] = constrains;
        }
        return j;
    }

    nlohmann::json PackageInfo::json_signable() const
    {
        nlohmann::json j;

        // Mandatory keys
        j["name"] = name;
        j["version"] = version;
        j["subdir"] = subdir;
        j["size"] = size;
        j["timestamp"] = timestamp;
        j["build"] = build_string;
        j["build_number"] = build_number;
        j["license"] = license;
        j["md5"] = md5;
        j["sha256"] = sha256;

        // Defaulted keys to empty arrays
        if (depends.empty())
        {
            if (defaulted_keys.find("depends") == defaulted_keys.end())
            {
                j["depends"] = nlohmann::json::array();
            }
        }
        else
        {
            j["depends"] = depends;
        }
        if (constrains.empty())
        {
            if (defaulted_keys.find("constrains") == defaulted_keys.end())
            {
                j["constrains"] = nlohmann::json::array();
            }
        }
        else
        {
            j["constrains"] = constrains;
        }

        return j;
    }

    std::string PackageInfo::str() const
    {
        return concat(name, "-", version, "-", build_string);
    }

    std::string PackageInfo::long_str() const
    {
        // TODO channel contains subdir right now?!
        return concat(channel, "::", name, "-", version, "-", build_string);
    }
}  // namespace mamba
