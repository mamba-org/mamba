// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "mamba/specs/repo_data.hpp"

NLOHMANN_JSON_NAMESPACE_BEGIN
template <typename T>
struct adl_serializer<std::optional<T>>
{
    static void to_json(json& j, const std::optional<T>& opt)
    {
        if (opt.has_value())
        {
            j = opt.value();
        }
        else
        {
            j = nullptr;
        }
    }

    static void from_json(const json& j, std::optional<T>& opt)
    {
        if (!j.is_null())
        {
            opt = j.template get<T>();
        }
        else
        {
            opt = std::nullopt;
        }
    }
};
NLOHMANN_JSON_NAMESPACE_END

namespace mamba::specs
{
    NLOHMANN_JSON_SERIALIZE_ENUM(
        NoArchType,
        {
            { NoArchType::Generic, "generic" },
            { NoArchType::Python, "python" },
        }
    )

    void to_json(nlohmann::json& j, const RepoDataPackage& p)
    {
        j["name"] = p.name;
        j["version"] = p.version.str();
        j["build"] = p.build_string;
        j["build_number"] = p.build_number;
        j["subdir"] = p.subdir;
        j["md5"] = p.md5;
        j["sha256"] = p.sha256;
        j["legacy_bz2_md5"] = p.legacy_bz2_md5;
        j["legacy_bz2_size"] = p.legacy_bz2_size;
        j["size"] = p.size;
        j["arch"] = p.arch;
        j["platform"] = p.platform;
        j["depends"] = p.depends;
        j["constrains"] = p.constrains;
        j["track_features"] = p.track_features;
        j["features"] = p.features;
        j["noarch"] = p.noarch;
        j["license"] = p.license;
        j["license_family"] = p.license_family;
        j["timestamp"] = p.timestamp;
    }

    namespace
    {
        template <typename Json, std::size_t N, typename T>
        void deserialize_maybe_missing(Json&& j, const char (&name)[N], T& t)
        {
            if (j.contains(name))
            {
                t = std::forward<Json>(j)[name].template get<T>();
            }
            else
            {
                t = {};
            }
        }
    }

    void from_json(const nlohmann::json& j, RepoDataPackage& p)
    {
        p.name = j.at("name");
        p.version = Version::parse(j.at("version").template get<std::string_view>());
        p.build_string = j.at("build");
        p.build_number = j.at("build_number");
        deserialize_maybe_missing(j, "subdir", p.subdir);
        deserialize_maybe_missing(j, "md5", p.md5);
        deserialize_maybe_missing(j, "sha256", p.sha256);
        deserialize_maybe_missing(j, "legacy_bz2_md5", p.legacy_bz2_md5);
        deserialize_maybe_missing(j, "legacy_bz2_size", p.legacy_bz2_size);
        deserialize_maybe_missing(j, "size", p.size);
        deserialize_maybe_missing(j, "arch", p.arch);
        deserialize_maybe_missing(j, "platform", p.platform);
        deserialize_maybe_missing(j, "depends", p.depends);
        deserialize_maybe_missing(j, "constrains", p.constrains);
        if (j.contains("track_features"))
        {
            auto&& track_features = j["track_features"];
            if (track_features.is_array())
            {
                p.track_features = std::forward<decltype(track_features)>(track_features);
            }
            // Consider a single element
            else if (track_features.is_string())
            {
                p.track_features.push_back(std::forward<decltype(track_features)>(track_features));
            }
        }
        deserialize_maybe_missing(j, "features", p.features);
        if (j.contains("noarch") && !j["noarch"].is_null())
        {
            auto&& noarch = j["noarch"];
            // old behaviour
            if (noarch.is_boolean())
            {
                if (noarch.template get<bool>())
                {
                    p.noarch = NoArchType::Generic;
                }
            }
            else
            {
                // Regular enum deserialization
                p.noarch = std::forward<decltype(noarch)>(noarch);
            }
        }
        deserialize_maybe_missing(j, "license", p.license);
        deserialize_maybe_missing(j, "license_family", p.license_family);
        deserialize_maybe_missing(j, "timestamp", p.timestamp);
    }

    void to_json(nlohmann::json& j, const ChannelInfo& info)
    {
        j["subdir"] = info.subdir;
    }

    void from_json(const nlohmann::json& j, ChannelInfo& info)
    {
        info.subdir = j["subdir"];
    }

    void to_json(nlohmann::json& j, const RepoData& data)
    {
        j["version"] = data.version;
        j["info"] = data.info;
        j["packages"] = data.packages;
        j["packages.conda"] = data.conda_packages;
        j["removed"] = data.removed;
    }

    void from_json(const nlohmann::json& j, RepoData& data)
    {
        deserialize_maybe_missing(j, "version", data.version);
        deserialize_maybe_missing(j, "info", data.info);
        deserialize_maybe_missing(j, "packages", data.packages);
        deserialize_maybe_missing(j, "packages.conda", data.conda_packages);
        deserialize_maybe_missing(j, "removed", data.removed);
    }
}
