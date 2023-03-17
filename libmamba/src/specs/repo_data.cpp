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
    template <typename Opt>
    static void to_json_impl(json& j, Opt&& opt)
    {
        if (opt.has_value())
        {
            j = std::forward<Opt>(opt).value();
        }
        else
        {
            j = nullptr;
        }
    }

    static void to_json(json& j, const std::optional<T>& opt)
    {
        return to_json_impl(j, opt);
    }

    static void to_json(json& j, std::optional<T>&& opt)
    {
        return to_json_impl(j, std::move(opt));
    }

    template <typename Json>
    static void from_json_impl(Json&& j, std::optional<T>& opt)
    {
        if (!j.is_null())
        {
            opt = std::forward<Json>(j).template get<T>();
        }
        else
        {
            opt = std::nullopt;
        }
    }

    static void from_json(const json& j, std::optional<T>& opt)
    {
        return from_json_impl(j, opt);
    }

    static void from_json(json&& j, std::optional<T>& opt)
    {
        return from_json_impl(std::move(j), opt);
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

    namespace
    {
        template <typename PackRec>
        void to_json_PackageRecord_impl(nlohmann::json& j, PackRec&& p)
        {
            j["name"] = std::forward<PackRec>(p).name;
            j["version"] = p.version.str();
            j["build"] = std::forward<PackRec>(p).build;
            j["build_number"] = std::forward<PackRec>(p).build_number;
            j["subdir"] = std::forward<PackRec>(p).subdir;
            j["md5"] = std::forward<PackRec>(p).md5;
            j["sha256"] = std::forward<PackRec>(p).sha256;
            j["legacy_bz2_md5"] = std::forward<PackRec>(p).legacy_bz2_md5;
            j["legacy_bz2_size"] = std::forward<PackRec>(p).legacy_bz2_size;
            j["size"] = std::forward<PackRec>(p).size;
            j["arch"] = std::forward<PackRec>(p).arch;
            j["platform"] = std::forward<PackRec>(p).platform;
            j["depends"] = std::forward<PackRec>(p).depends;
            j["constrains"] = std::forward<PackRec>(p).constrains;
            j["track_features"] = std::forward<PackRec>(p).track_features;
            j["features"] = std::forward<PackRec>(p).features;
            j["noarch"] = std::forward<PackRec>(p).noarch;
            j["license"] = std::forward<PackRec>(p).license;
            j["license_family"] = std::forward<PackRec>(p).license_family;
            j["timestamp"] = std::forward<PackRec>(p).timestamp;
        }
    }

    void to_json(nlohmann::json& j, const PackageRecord& p)
    {
        return to_json_PackageRecord_impl(j, p);
    }

    void to_json(nlohmann::json& j, PackageRecord&& p)
    {
        return to_json_PackageRecord_impl(j, std::move(p));
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

        template <typename Json>
        void from_json_PackageRecord_impl(Json&& j, PackageRecord& p)
        {
            p.name = std::forward<Json>(j).at("name");
            p.version = Version::parse(j.at("version").template get<std::string_view>());
            p.build = std::forward<Json>(j).at("build");
            p.build_number = std::forward<Json>(j).at("build_number");
            p.subdir = std::forward<Json>(j).at("subdir");
            deserialize_maybe_missing(std::forward<Json>(j), "md5", p.md5);
            deserialize_maybe_missing(std::forward<Json>(j), "sha256", p.sha256);
            deserialize_maybe_missing(std::forward<Json>(j), "legacy_bz2_md5", p.legacy_bz2_md5);
            deserialize_maybe_missing(std::forward<Json>(j), "legacy_bz2_size", p.legacy_bz2_size);
            deserialize_maybe_missing(std::forward<Json>(j), "size", p.size);
            deserialize_maybe_missing(std::forward<Json>(j), "arch", p.arch);
            deserialize_maybe_missing(std::forward<Json>(j), "platform", p.platform);
            deserialize_maybe_missing(std::forward<Json>(j), "depends", p.depends);
            deserialize_maybe_missing(std::forward<Json>(j), "constrains", p.constrains);
            if (j.contains("track_features"))
            {
                auto&& track_features = std::forward<Json>(j)["track_features"];
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
            deserialize_maybe_missing(std::forward<Json>(j), "features", p.features);
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
            deserialize_maybe_missing(std::forward<Json>(j), "license", p.license);
            deserialize_maybe_missing(std::forward<Json>(j), "license_family", p.license_family);
            deserialize_maybe_missing(std::forward<Json>(j), "timestamp", p.timestamp);
        }
    }

    void from_json(const nlohmann::json& j, PackageRecord& p)
    {
        return from_json_PackageRecord_impl(j, p);
    }

    void from_json(nlohmann::json&& j, PackageRecord& p)
    {
        return from_json_PackageRecord_impl(std::move(j), p);
    }
}
