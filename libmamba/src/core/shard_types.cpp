// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/shard_types.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/specs/platform.hpp"
#include "mamba/specs/version.hpp"
#include "mamba/util/encoding.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba
{
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

        return specs::RepoData{ .version = repodata.repodata_version,
                                .info = info_value,
                                .packages = repodata.shard_dict.packages,
                                .conda_packages = repodata.shard_dict.conda_packages };
    }

    specs::PackageInfo to_package_info(
        const specs::RepoDataPackage& record,
        const std::string& filename,
        const std::string& channel_id,
        const specs::DynamicPlatform& platform,
        const std::string& base_url
    )
    {
        const specs::NoArchType noarch_value = record.noarch.value_or(specs::NoArchType::No);

        specs::PackageInfo pkg_info;
        pkg_info.name = record.name;
        pkg_info.version = record.raw_version.value_or(record.version.to_string());
        pkg_info.build_string = record.build_string;
        pkg_info.build_number = record.build_number;
        pkg_info.channel = channel_id;
        pkg_info.package_url = util::url_concat(base_url, "/", util::encode_percent(filename));
        pkg_info.platform = platform;
        pkg_info.filename = filename;
        pkg_info.license = record.license.value_or("");
        pkg_info.md5 = record.md5.value_or("");
        pkg_info.sha256 = record.sha256.value_or("");
        pkg_info.dependencies = record.depends;
        pkg_info.constrains = record.constrains;
        pkg_info.track_features = record.track_features;
        pkg_info.noarch = noarch_value;
        pkg_info.size = record.size.value_or(0);
        pkg_info.timestamp = record.timestamp.value_or(0);
        return pkg_info;
    }
}
