// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/shard_types.hpp"
#include "mamba/specs/platform.hpp"
#include "mamba/specs/version.hpp"

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
}
