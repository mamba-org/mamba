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
        specs::RepoDataPackage pkg;
        pkg.name = record.name;
        // Parse version string to Version object
        if (auto version = specs::Version::parse(record.version))
        {
            pkg.version = version.value();
        }
        pkg.build_string = record.build;
        pkg.build_number = record.build_number;
        pkg.sha256 = record.sha256;
        pkg.md5 = record.md5;
        pkg.depends = record.depends;
        pkg.constrains = record.constrains;
        if (record.noarch)
        {
            // Parse noarch string to NoArchType
            const auto& noarch_str = *record.noarch;
            if (noarch_str == "python")
            {
                pkg.noarch = specs::NoArchType::Python;
            }
            else if (noarch_str == "generic")
            {
                pkg.noarch = specs::NoArchType::Generic;
            }
            // If noarch_str doesn't match known values, leave pkg.noarch as nullopt
        }
        return pkg;
    }

    ShardPackageRecord from_repo_data_package(const specs::RepoDataPackage& record)
    {
        ShardPackageRecord shard_record;
        shard_record.name = record.name;
        shard_record.version = record.version.to_string();
        shard_record.build = record.build_string;
        shard_record.build_number = record.build_number;
        shard_record.sha256 = record.sha256;
        shard_record.md5 = record.md5;
        shard_record.depends = record.depends;
        shard_record.constrains = record.constrains;
        if (record.noarch)
        {
            switch (*record.noarch)
            {
                case specs::NoArchType::Generic:
                    shard_record.noarch = "generic";
                    break;
                case specs::NoArchType::Python:
                    shard_record.noarch = "python";
                    break;
                case specs::NoArchType::No:
                default:
                    // No noarch type
                    break;
            }
        }
        return shard_record;
    }

    specs::RepoData to_repo_data(const RepodataDict& repodata)
    {
        specs::RepoData result;
        result.version = repodata.repodata_version;

        // Convert info section
        if (!repodata.info.subdir.empty())
        {
            specs::ChannelInfo channel_info;
            // Parse subdir to KnownPlatform if possible
            if (auto platform = specs::platform_parse(repodata.info.subdir))
            {
                channel_info.subdir = platform.value();
            }
            result.info = channel_info;
        }

        // Convert packages
        for (const auto& [filename, record] : repodata.packages)
        {
            result.packages[filename] = to_repo_data_package(record);
        }

        // Convert conda packages
        for (const auto& [filename, record] : repodata.conda_packages)
        {
            result.conda_packages[filename] = to_repo_data_package(record);
        }

        return result;
    }

    specs::PackageInfo to_package_info(
        const ShardPackageRecord& record,
        const std::string& filename,
        const std::string& channel_id,
        const specs::DynamicPlatform& platform,
        const std::string& base_url
    )
    {
        specs::PackageInfo pkg_info;
        pkg_info.name = record.name;
        pkg_info.version = record.version;
        pkg_info.build_string = record.build;
        pkg_info.build_number = record.build_number;
        pkg_info.filename = filename;
        pkg_info.channel = channel_id;
        pkg_info.platform = platform;
        pkg_info.package_url = util::url_concat(base_url, "/", filename);
        pkg_info.dependencies = record.depends;
        pkg_info.constrains = record.constrains;
        pkg_info.size = record.size;
        if (record.sha256)
        {
            pkg_info.sha256 = *record.sha256;
        }
        if (record.md5)
        {
            pkg_info.md5 = *record.md5;
        }
        if (record.noarch)
        {
            if (*record.noarch == "python")
            {
                pkg_info.noarch = specs::NoArchType::Python;
            }
            else if (*record.noarch == "generic")
            {
                pkg_info.noarch = specs::NoArchType::Generic;
            }
        }
        return pkg_info;
    }
}
