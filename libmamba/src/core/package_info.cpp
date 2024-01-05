// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <functional>
#include <tuple>
#include <type_traits>

#include <fmt/core.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "mamba/core/package_info.hpp"
#include "mamba/specs/archive.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{

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

    namespace
    {
        template <typename T, typename U>
        auto contains(const std::vector<T>& v, const U& val)
        {
            return std::find(v.cbegin(), v.cend(), val) != v.cend();
        }
    }

    auto PackageInfo::json_signable() const -> nlohmann::json
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
        if (!noarch.empty())
        {
            j["noarch"] = noarch;
        }
        j["license"] = license;
        j["md5"] = md5;
        j["sha256"] = sha256;

        // Defaulted keys to empty arrays
        if (depends.empty())
        {
            if (!contains(defaulted_keys, "depends"))
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
            if (!contains(defaulted_keys, "constrains"))
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

    auto PackageInfo::str() const -> std::string
    {
        if (!fn.empty())
        {
            return std::string(specs::strip_archive_extension(fn));
        }
        return fmt::format("{}-{}-{}", name, version, build_string);
    }

    auto PackageInfo::long_str() const -> std::string
    {
        // TODO channel contains subdir right now?!
        return util::concat(channel, "::", str());
    }

    namespace
    {
        template <typename Func>
        auto invoke_field_string(const PackageInfo& p, Func&& field) -> std::string
        {
            using Out = std::decay_t<std::invoke_result_t<Func, PackageInfo>>;

            if constexpr (std::is_same_v<Out, const char*>)
            {
                return std::string{ std::invoke(field, p) };
            }
            else if constexpr (std::is_integral_v<Out> || std::is_floating_point_v<Out>)
            {
                return std::to_string(std::invoke(field, p));
            }
            else if constexpr (std::is_convertible_v<Out, std::string>)
            {
                return static_cast<std::string>(std::invoke(field, p));
            }
            else if constexpr (std::is_constructible_v<Out, std::string>)
            {
                return std::string(std::invoke(field, p));
            }
            else if constexpr (fmt::is_formattable<Out>::value)
            {
                return fmt::format("{}", std::invoke(field, p));
            }
            return "";
        }
    }

    auto PackageInfo::field(std::string_view field_name) const -> std::string
    {
        field_name = util::strip(field_name);
        if (field_name == "name")
        {
            return invoke_field_string(*this, &PackageInfo::name);
        }
        if (field_name == "version")
        {
            return invoke_field_string(*this, &PackageInfo::version);
        }
        if (field_name == "build_string")
        {
            return invoke_field_string(*this, &PackageInfo::build_string);
        }
        if (field_name == "build_number")
        {
            return invoke_field_string(*this, &PackageInfo::build_number);
        }
        if (field_name == "noarch")
        {
            return invoke_field_string(*this, &PackageInfo::noarch);
        }
        if (field_name == "channel")
        {
            return invoke_field_string(*this, &PackageInfo::channel);
        }
        if (field_name == "url")
        {
            return invoke_field_string(*this, &PackageInfo::url);
        }
        if (field_name == "subdir")
        {
            return invoke_field_string(*this, &PackageInfo::subdir);
        }
        if (field_name == "fn" || field_name == "filename")
        {
            return invoke_field_string(*this, &PackageInfo::fn);
        }
        if (field_name == "license")
        {
            return invoke_field_string(*this, &PackageInfo::license);
        }
        if (field_name == "size")
        {
            return invoke_field_string(*this, &PackageInfo::size);
        }
        if (field_name == "timestamp")
        {
            return invoke_field_string(*this, &PackageInfo::timestamp);
        }
        throw std::invalid_argument(fmt::format(R"(Invalid field "{}")", field_name));
    }

    namespace
    {
        auto attrs(const PackageInfo& p)
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
        }
    }

    auto operator==(const PackageInfo& lhs, const PackageInfo& rhs) -> bool
    {
        return attrs(lhs) == attrs(rhs);
    }

    auto operator!=(const PackageInfo& lhs, const PackageInfo& rhs) -> bool
    {
        return !(lhs == rhs);
    }

    void to_json(nlohmann::json& j, const PackageInfo& pkg)
    {
        j["name"] = pkg.name;
        j["version"] = pkg.version;
        j["channel"] = pkg.channel;
        j["url"] = pkg.url;
        j["subdir"] = pkg.subdir;
        j["fn"] = pkg.fn;
        j["size"] = pkg.size;
        j["timestamp"] = pkg.timestamp;
        j["build"] = pkg.build_string;
        j["build_string"] = pkg.build_string;
        j["build_number"] = pkg.build_number;
        if (!pkg.noarch.empty())
        {
            j["noarch"] = pkg.noarch;
        }
        j["license"] = pkg.license;
        j["track_features"] = fmt::format("{}", fmt::join(pkg.track_features, ","));
        if (!pkg.md5.empty())
        {
            j["md5"] = pkg.md5;
        }
        if (!pkg.sha256.empty())
        {
            j["sha256"] = pkg.sha256;
        }
        if (pkg.depends.empty())
        {
            j["depends"] = nlohmann::json::array();
        }
        else
        {
            j["depends"] = pkg.depends;
        }

        if (pkg.constrains.empty())
        {
            j["constrains"] = nlohmann::json::array();
        }
        else
        {
            j["constrains"] = pkg.constrains;
        }
    }

    void from_json(const nlohmann::json& j, PackageInfo& pkg)
    {
        pkg.name = j.value("name", "");
        pkg.version = j.value("version", "");
        pkg.channel = j.value("channel", "");
        pkg.url = j.value("url", "");
        pkg.subdir = j.value("subdir", "");
        pkg.fn = j.value("fn", "");
        pkg.size = j.value("size", std::size_t(0));
        pkg.timestamp = j.value("timestamp", std::size_t(0));
        if (std::string build = j.value("build", "<UNKNOWN>"); build != "<UNKNOWN>")
        {
            pkg.build_string = std::move(build);
        }
        else
        {
            pkg.build_string = j.value("build_string", "");
        }
        pkg.build_number = j.value("build_number", std::size_t(0));
        pkg.license = j.value("license", "");
        pkg.md5 = j.value("md5", "");
        pkg.sha256 = j.value("sha256", "");
        if (std::string feat = j.value("track_features", ""); !feat.empty())
        {
            // Split empty string would have an empty element
            pkg.track_features = util::split(feat, ",");
        }

        // add the noarch type if we know it (only known for installed packages)
        if (j.contains("noarch"))
        {
            if (j["noarch"].type() == nlohmann::json::value_t::boolean)
            {
                pkg.noarch = "generic_v1";
            }
            else
            {
                pkg.noarch = j.value("noarch", "");
            }
        }

        pkg.depends = j.value("depends", std::vector<std::string>());
        pkg.constrains = j.value("constrains", std::vector<std::string>());
    }
}
