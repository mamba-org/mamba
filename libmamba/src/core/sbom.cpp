// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "mamba/core/sbom.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/specs/platform.hpp"
#include "mamba/util/encoding.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba::sbom
{
    namespace detail
    {
        inline constexpr std::string_view default_repository_url = "https://conda.anaconda.org";
        inline constexpr std::string_view default_repository_host = "conda.anaconda.org";

        auto archive_type(const specs::PackageInfo& pkg) -> std::string_view
        {
            const auto& filename = pkg.filename.empty() ? pkg.package_url : pkg.filename;
            if (filename.ends_with(".tar.bz2"))
            {
                return "tar.bz2";
            }
            if (filename.ends_with(".conda"))
            {
                return "conda";
            }
            return {};
        }

        auto channel_home_url(const specs::PackageInfo& pkg) -> std::optional<specs::CondaURL>
        {
            auto try_parse = [](std::string_view str) -> std::optional<specs::CondaURL>
            {
                // Require a scheme so bare channel names are not parsed as `https` hosts.
                if (!util::url_has_scheme(str))
                {
                    return std::nullopt;
                }
                auto parsed = specs::CondaURL::parse(str);
                if (!parsed.has_value())
                {
                    return std::nullopt;
                }
                return std::move(parsed.value());
            };

            auto url = try_parse(pkg.package_url);
            if (!url.has_value())
            {
                url = try_parse(pkg.channel);
            }
            if (!url.has_value())
            {
                return std::nullopt;
            }
            url->clear_token();
            url->clear_user();
            url->clear_password();
            url->clear_query();
            url->clear_fragment();
            url->clear_package();
            url->clear_platform();
            if (const auto path = url->path(specs::CondaURL::Decode::no); path.size() > 1)
            {
                url->set_path(std::string{ util::rstrip(path, '/') }, specs::CondaURL::Encode::no);
            }
            return url;
        }

        namespace
        {
            // CEP-49: "defaults" and "local" are not allowed, and a final namespace segment
            // matching a channel subdir name would lead to ambiguous PURLs.
            auto namespace_from_segment(std::string_view segment) -> std::optional<std::string>
            {
                const auto ns = util::to_lower(segment);
                if (ns.empty() || (ns.find('/') != std::string_view::npos) || (ns == "defaults")
                    || (ns == "local") || specs::platform_parse(ns).has_value())
                {
                    return std::nullopt;
                }
                return ns;
            }
        }

        auto namespace_from_channel_home(const specs::CondaURL& home) -> std::optional<std::string>
        {
            if (util::to_lower(home.host()) != default_repository_host)
            {
                return std::nullopt;
            }
            const auto path = std::string{ util::strip(home.path(), '/') };
            // Allow `namespace` to be a single-segment only to keep it simple.
            // Multi-segment paths fall back to use qualifiers `repository_url` instead.
            // This may change depending on future use cases.
            return namespace_from_segment(path);
        }

        auto channel_namespace(const specs::PackageInfo& pkg) -> std::optional<std::string>
        {
            if (const auto home = channel_home_url(pkg))
            {
                return namespace_from_channel_home(*home);
            }
            return namespace_from_segment(pkg.channel);
        }

        auto channel_repository_url(const specs::PackageInfo& pkg) -> std::string
        {
            if (const auto home = channel_home_url(pkg))
            {
                return home->pretty_str(
                    specs::CondaURL::StripScheme::no,
                    '/',
                    specs::CondaURL::Credentials::Remove
                );
            }
            if (!pkg.channel.empty() && (pkg.channel.find("://") != std::string_view::npos))
            {
                return std::string{ util::rstrip(pkg.channel, '/') };
            }
            // Bare channel names are relative to the default repository.
            const auto channel = util::rstrip(pkg.channel, '/');
            if (channel.empty())
            {
                return std::string{ default_repository_url };
            }
            return util::concat(default_repository_url, "/", channel);
        }

        // Qualifiers: build, build_number, subdir, type, checksum, repository_url.
        auto
        collect_qualifiers(const specs::PackageInfo& pkg, std::optional<std::string_view> repository_url)
            -> std::string
        {
            // TODO Handle `vers` and `download_url` (later if necessary?)
            // Maybe more when consuming PURLs in `mamba`
            // NOTE `vers` qualifier and `version` component are mutually exclusive
            // NOTE `download_url` should not conflict with other info in PURL components
            auto quals = std::vector<std::string>{};
            if (!pkg.build_string.empty())
            {
                quals.push_back(fmt::format("build={}", util::encode_percent(pkg.build_string)));
            }
            quals.push_back(fmt::format("build_number={}", pkg.build_number));
            if (!pkg.platform.empty())
            {
                quals.push_back(fmt::format("subdir={}", util::encode_percent(pkg.platform)));
            }
            if (const auto type = archive_type(pkg); !type.empty())
            {
                quals.push_back(fmt::format("type={}", type));
            }
            if (!pkg.sha256.empty())
            {
                // CEP-49: the checksum algorithm name and hexadecimal value must be lowercase.
                // NOTE `sha256` is recommended but we can add other algorithms if needed later.
                quals.push_back(fmt::format("checksum=sha256:{}", util::to_lower(pkg.sha256)));
            }
            if (repository_url.has_value())
            {
                quals.push_back(
                    fmt::format("repository_url={}", util::encode_percent(*repository_url))
                );
            }
            return quals.empty() ? std::string{} : util::concat("?", util::join("&", quals));
        }
    }

    // A PURL is a URL composed of seven components:
    // scheme:type/namespace/name@version?qualifiers#subpath
    auto make_conda_purl(const specs::PackageInfo& pkg) -> std::string
    {
        const auto name = util::encode_percent(util::to_lower(pkg.name));
        const auto version = util::encode_percent(pkg.version);

        if (const auto ns = detail::channel_namespace(pkg))
        {
            return util::concat(
                "pkg:conda/",
                *ns,
                "/",
                name,
                "@",
                version,
                detail::collect_qualifiers(pkg, std::nullopt)
            );
        }
        const auto repository_url = detail::channel_repository_url(pkg);
        return util::concat(
            "pkg:conda/",
            name,
            "@",
            version,
            detail::collect_qualifiers(pkg, std::string_view{ repository_url })
        );
    }
}
