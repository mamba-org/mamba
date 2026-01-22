// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/shard_types.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/specs/platform.hpp"
#include "mamba/specs/version.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba
{
    specs::RepoDataPackage to_repo_data_package(const ShardPackageRecord& record)
    {
        // Parse version string to Version object
        auto version = specs::Version::parse(record.version);
        auto parsed_version = version ? version.value() : specs::Version(0, { { { 0 } } });

        // Parse noarch string to NoArchType
        std::optional<specs::NoArchType> noarch_value = std::nullopt;
        if (record.noarch)
        {
            const auto& noarch_str = *record.noarch;
            if (noarch_str == "python")
            {
                noarch_value = specs::NoArchType::Python;
            }
            else if (noarch_str == "generic")
            {
                noarch_value = specs::NoArchType::Generic;
            }
            // If noarch_str doesn't match known values, leave as nullopt
        }

        return specs::RepoDataPackage{ .name = record.name,
                                       .version = parsed_version,
                                       .build_string = record.build,
                                       .build_number = record.build_number,
                                       .subdir = record.subdir,
                                       .md5 = record.md5,
                                       .sha256 = record.sha256,
                                       .python_site_packages_path = {},
                                       .legacy_bz2_md5 = {},
                                       .legacy_bz2_size = {},
                                       .size = record.size > 0
                                                   ? std::optional<std::size_t>(record.size)
                                                   : std::nullopt,
                                       .arch = {},
                                       .platform = {},
                                       .depends = record.depends,
                                       .constrains = record.constrains,
                                       .track_features = {},
                                       .features = {},
                                       .noarch = noarch_value,
                                       .license = record.license,
                                       .license_family = record.license_family,
                                       .timestamp = record.timestamp };
    }

    ShardPackageRecord from_repo_data_package(const specs::RepoDataPackage& record)
    {
        std::optional<std::string> noarch_value = std::nullopt;
        if (record.noarch)
        {
            switch (*record.noarch)
            {
                case specs::NoArchType::Generic:
                    noarch_value = "generic";
                    break;
                case specs::NoArchType::Python:
                    noarch_value = "python";
                    break;
                case specs::NoArchType::No:
                default:
                    // No noarch type
                    break;
            }
        }

        return ShardPackageRecord{ .name = record.name,
                                   .version = record.version.to_string(),
                                   .build = record.build_string,
                                   .build_number = record.build_number,
                                   .sha256 = record.sha256,
                                   .md5 = record.md5,
                                   .depends = record.depends,
                                   .constrains = record.constrains,
                                   .noarch = noarch_value,
                                   .size = record.size.value_or(0),
                                   .license = record.license,
                                   .license_family = record.license_family,
                                   .subdir = record.subdir,
                                   .timestamp = record.timestamp };
    }

    specs::RepoData to_repo_data(const RepodataDict& repodata)
    {
        // Convert info section
        std::optional<specs::ChannelInfo> info_value = std::nullopt;
        if (!repodata.info.subdir.empty())
        {
            // Parse subdir to KnownPlatform if possible
            if (auto platform = specs::platform_parse(repodata.info.subdir))
            {
                info_value = specs::ChannelInfo{ .subdir = platform.value() };
            }
        }

        // Convert packages
        std::map<std::string, specs::RepoDataPackage> packages;
        for (const auto& [filename, record] : repodata.packages)
        {
            packages[filename] = to_repo_data_package(record);
        }

        // Convert conda packages
        std::map<std::string, specs::RepoDataPackage> conda_packages;
        for (const auto& [filename, record] : repodata.conda_packages)
        {
            conda_packages[filename] = to_repo_data_package(record);
        }

        return specs::RepoData{ .version = repodata.repodata_version,
                                .info = info_value,
                                .packages = std::move(packages),
                                .conda_packages = std::move(conda_packages) };
    }

    specs::PackageInfo to_package_info(
        const ShardPackageRecord& record,
        const std::string& filename,
        const std::string& channel_id,
        const specs::DynamicPlatform& platform,
        const std::string& base_url
    )
    {
        specs::NoArchType noarch_value = specs::NoArchType::No;
        if (record.noarch)
        {
            if (*record.noarch == "python")
            {
                noarch_value = specs::NoArchType::Python;
            }
            else if (*record.noarch == "generic")
            {
                noarch_value = specs::NoArchType::Generic;
            }
        }

        specs::PackageInfo pkg_info;
        pkg_info.name = record.name;
        pkg_info.version = record.version;
        pkg_info.build_string = record.build;
        pkg_info.build_number = record.build_number;
        pkg_info.channel = channel_id;
        pkg_info.package_url = util::url_concat(base_url, "/", filename);
        pkg_info.platform = platform;
        pkg_info.filename = filename;
        pkg_info.license = record.license.value_or("");
        pkg_info.md5 = record.md5.value_or("");
        pkg_info.sha256 = record.sha256.value_or("");
        pkg_info.dependencies = record.depends;
        pkg_info.constrains = record.constrains;
        pkg_info.noarch = noarch_value;
        pkg_info.size = record.size;
        pkg_info.timestamp = record.timestamp.value_or(0);
        return pkg_info;
    }
}
