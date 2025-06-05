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
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

#include "mamba/specs/archive.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba::specs
{
    namespace
    {
        auto parse_extension(std::string_view spec) -> PackageType
        {
            if (util::ends_with(spec, ".whl"))
            {
                return PackageType::Wheel;
            }
            else if (util::ends_with(spec, ".tar.gz"))
            {
                return PackageType::TarGz;
            }
            return PackageType::Conda;
        }

        auto parse_url(std::string_view spec) -> expected_parse_t<PackageInfo>
        {
            if (!has_archive_extension(spec))
            {
                return make_unexpected_parse("Missing filename extension.");
            }

            auto out = PackageInfo();
            // TODO decide on the best way to group filename/channel/subdir/package_url all at once
            out.package_url = util::path_or_url_to_url(spec);

            auto url = CondaURL();
            {
                auto maybe_url = CondaURL::parse(out.package_url);
                if (!maybe_url)
                {
                    return make_unexpected_parse(maybe_url.error());
                }
                url = std::move(maybe_url).value();
            }

            out.filename = url.package();
            url.clear_package();

            // The filename format depends on the package_type:
            out.package_type = parse_extension(spec);
            // PackageType::Conda (.tar.bz2 or .conda):
            // {pkg name}-{version}-{build string}.{tar.bz2, conda}
            if (out.package_type == PackageType::Conda)
            {
                out.platform = url.platform_name();
                url.clear_platform();
                out.channel = util::rstrip(url.str(specs::CondaURL::Credentials::Show), '/');

                // Note that we use `rsplit...` instead of `split...`
                // because the package name may contain '-'.
                // Build string
                auto [head, tail] = util::rsplit_once(strip_archive_extension(out.filename), '-');
                out.build_string = tail;
                if (!head.has_value())
                {
                    return make_unexpected_parse(
                        fmt::format(R"(Missing name and version in filename "{}".)", out.filename)
                    );
                }

                // Version
                std::tie(head, tail) = util::rsplit_once(head.value(), '-');
                out.version = tail;
                if (!head.has_value())
                {
                    return make_unexpected_parse(
                        fmt::format(R"(Missing name in filename "{}".)", out.filename)
                    );
                }

                // Name
                out.name = head.value();  // There may be '-' in the name
            }
            // PackageType::Wheel (.whl):
            // {pkg name}-{version}-{build tag (optional)}-{python tag}-{abi tag}-{platform tag}.whl
            // cf.
            // https://packaging.python.org/en/latest/specifications/platform-compatibility-tags/
            else if (out.package_type == PackageType::Wheel)
            {
                // Platform tag
                auto [head, tail] = util::rsplit_once(strip_archive_extension(out.filename), '-');
                if (!head.has_value())
                {
                    return make_unexpected_parse(
                        fmt::format(R"(Missing tags in filename "{}".)", out.filename)
                    );
                }
                // Abi tag
                std::tie(head, tail) = util::rsplit_once(head.value(), '-');
                if (!head.has_value())
                {
                    return make_unexpected_parse(
                        fmt::format(R"(Missing tags in filename "{}".)", out.filename)
                    );
                }
                // Python tag
                std::tie(head, tail) = util::rsplit_once(head.value(), '-');
                if (!head.has_value())
                {
                    return make_unexpected_parse(
                        fmt::format(R"(Missing tags in filename "{}".)", out.filename)
                    );
                }
                // Build tag or version
                std::tie(head, tail) = util::rsplit_once(head.value(), '-');
                if (!head.has_value())
                {
                    return make_unexpected_parse(
                        fmt::format(R"(Missing tags in filename "{}".)", out.filename)
                    );
                }
                if (util::contains(tail, '.'))
                {
                    // The tail is the version
                    out.version = tail;
                    // The head is the name
                    out.name = head.value();  // There may be '-' in the name
                }
                else
                {
                    // The previous tail is the optional build tag
                    std::tie(head, tail) = util::rsplit_once(head.value(), '-');
                    // The tail is the version
                    out.version = tail;
                    if (!head.has_value())
                    {
                        return make_unexpected_parse(
                            fmt::format(R"(Missing name in filename "{}".)", out.filename)
                        );
                    }

                    // Name
                    out.name = head.value();  // There may be '-' in the name
                }
            }
            // PackageType::TarGz (.tar.gz): {pkg name}-{version}.tar.gz
            else if (out.package_type == PackageType::TarGz)
            {
                // Version
                auto [head, tail] = util::rsplit_once(strip_archive_extension(out.filename), '-');
                out.version = tail;
                if (!head.has_value())
                {
                    return make_unexpected_parse(
                        fmt::format(R"(Missing name in filename "{}".)", out.filename)
                    );
                }

                // Name
                out.name = head.value();  // There may be '-' in the name
            }

            return out;
        }

        auto is_hash(std::string_view text) -> bool
        {
            constexpr auto is_hash_char = [](char c) -> bool
            {
                const auto lower = util::to_lower(c);
                return util::is_digit(c) || (lower == 'a') || (lower == 'b') || (lower == 'c')
                       || (lower == 'd') || (lower == 'e') || (lower == 'f');
            };
            return std::all_of(text.cbegin(), text.cend(), is_hash_char);
        }
    }

    auto PackageInfo::from_url(std::string_view str) -> expected_parse_t<PackageInfo>
    {
        str = util::strip(str);
        if (str.empty())
        {
            return {};
        }

        // A plain URL like https://conda.anaconda.org/conda-forge/linux-64/pkg-6.4-bld.conda
        if (has_archive_extension(str))
        {
            return parse_url(str);
        }

        // A URL with hash, generated by `mamba env export --explicit` like
        // https://conda.anaconda.org/conda-forge/linux-64/pkg-6.4-bld.conda#7dbaa197d7ba6032caf7ae7f32c1efa0
        if (const auto idx = str.rfind('#'); idx != std::string_view::npos)
        {
            auto url = str.substr(0, idx);
            auto hash = str.substr(idx + 1);
            if (has_archive_extension(url))
            {
                return parse_url(url).transform(
                    [&](PackageInfo&& pkg) -> PackageInfo
                    {
                        if (util::starts_with(hash, "sha256:"))
                        {
                            hash = hash.substr(7);
                            if (hash.size() == 64 && is_hash(hash))
                            {
                                pkg.sha256 = hash;
                            }
                        }
                        else if (is_hash(hash))
                        {
                            if (hash.size() == 32)
                            {
                                pkg.md5 = hash;
                            }
                            else if (hash.size() == 64)
                            {
                                pkg.sha256 = hash;
                            }
                        }
                        return pkg;
                    }
                );
            }
        }

        // A git repository URL over https and used by `pip`
        // git+https://<repository-url>@<commit|branch|tag>#egg=<package-name>
        if (util::starts_with(str, "git+https"))
        {
            auto pkg = PackageInfo();
            pkg.package_url = str;
            const std::string pkg_name_marker = "#egg=";
            if (const auto idx = str.rfind(pkg_name_marker); idx != std::string_view::npos)
            {
                pkg.name = str.substr(idx + pkg_name_marker.length());
            }
            return pkg;
        }

        return make_unexpected_parse(fmt::format(R"(Fail to parse PackageInfo URL "{}")", str));
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

    PackageInfo::PackageInfo(std::string n, std::string v, std::string b, std::string c)
        : name(std::move(n))
        , version(std::move(v))
        , build_string(std::move(b))
        , channel(std::move(c))
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
        j["subdir"] = platform;
        j["size"] = size;
        j["timestamp"] = timestamp;
        j["build"] = build_string;
        j["build_number"] = build_number;
        j["license"] = license;
        j["md5"] = md5;
        j["sha256"] = sha256;

        // Defaulted keys to empty arrays
        if (dependencies.empty())
        {
            if (!contains(defaulted_keys, "depends"))
            {
                j["depends"] = nlohmann::json::array();
            }
        }
        else
        {
            j["depends"] = dependencies;
        }

        // NOTE `constrains` is not included in server side (i.e Quetz)
        // If it is later (or is included within signed metadata even as
        // an empty array on conda side for example)
        // => do the same as "depends" above
        if (!constrains.empty())
        {
            j["constrains"] = constrains;
        }

        return j;
    }

    auto PackageInfo::str() const -> std::string
    {
        if (!filename.empty())
        {
            return std::string(specs::strip_archive_extension(filename));
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
            return std::string(noarch_name(noarch));
        }
        if (field_name == "channel")
        {
            return invoke_field_string(*this, &PackageInfo::channel);
        }
        if (field_name == "package_url" || field_name == "url")
        {
            return invoke_field_string(*this, &PackageInfo::package_url);
        }
        if (field_name == "subdir")
        {
            return invoke_field_string(*this, &PackageInfo::platform);
        }
        if (field_name == "fn" || field_name == "filename")
        {
            return invoke_field_string(*this, &PackageInfo::filename);
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
                p.package_url,
                p.platform,
                p.filename,
                p.license,
                p.size,
                p.timestamp,
                p.md5,
                p.sha256,
                p.track_features,
                p.dependencies,
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
        j["url"] = pkg.package_url;  // The conda key name
        j["subdir"] = pkg.platform;
        j["fn"] = pkg.filename;  // The conda key name
        j["size"] = pkg.size;
        j["timestamp"] = pkg.timestamp;
        j["build"] = pkg.build_string;
        j["build_string"] = pkg.build_string;
        j["build_number"] = pkg.build_number;
        if (pkg.noarch != NoArchType::No)
        {
            j["noarch"] = pkg.noarch;
        }
        j["license"] = pkg.license;
        j["track_features"] = fmt::format("{}", fmt::join(pkg.track_features, ","));  // Conda fmt
        if (!pkg.md5.empty())
        {
            j["md5"] = pkg.md5;
        }
        if (!pkg.sha256.empty())
        {
            j["sha256"] = pkg.sha256;
        }
        if (!pkg.signatures.empty())
        {
            j["signatures"] = pkg.signatures;
        }
        if (pkg.dependencies.empty())
        {
            j["depends"] = nlohmann::json::array();
        }
        else
        {
            j["depends"] = pkg.dependencies;
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
        pkg.package_url = j.value("url", "");
        pkg.platform = j.value("subdir", "");
        pkg.filename = j.value("fn", "");
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
        pkg.signatures = j.value("signatures", "");
        if (auto it = j.find("track_features"); it != j.end())
        {
            if (it->is_string() && !it->get<std::string_view>().empty())
            {
                // Split empty string would have an empty element
                pkg.track_features = util::split(it->get<std::string_view>(), ",");
            }
            if (it->is_array())
            {
                pkg.track_features.reserve(it->size());
                for (const auto& elem : *it)
                {
                    pkg.track_features.emplace_back(elem);
                }
            }
        }

        // add the noarch type if we know it (only known for installed packages)
        if (auto it = j.find("noarch"); it != j.end())
        {
            pkg.noarch = *it;
        }

        pkg.dependencies = j.value("depends", std::vector<std::string>());
        pkg.constrains = j.value("constrains", std::vector<std::string>());
    }
}
