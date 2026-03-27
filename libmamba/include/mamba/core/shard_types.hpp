// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SHARD_TYPES_HPP
#define MAMBA_CORE_SHARD_TYPES_HPP

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "mamba/specs/package_info.hpp"
#include "mamba/specs/repo_data.hpp"

namespace mamba
{
    /**
     * A shard dictionary containing packages for a single package name.
     *
     * Maps to the Python ShardDict type. Contains all versions of a package
     * in both .tar.bz2 and .conda formats.
     *
     * Shards use a msgpack format; parsing is handled in `shards.cpp`.
     * We store entries as `specs::RepoDataPackage` directly, which is the canonical
     * record type used across libmamba.
     *
     * @see specs::RepoDataPackage The canonical package record type used for repodata.json
     * @see https://conda.org/learn/ceps/cep-0016 CEP 16 - Sharded Repodata specification
     */
    struct ShardDict
    {
        /** Packages in .tar.bz2 format, keyed by filename. */
        std::map<std::string, specs::RepoDataPackage> packages;

        /** Packages in .conda format, keyed by filename. */
        std::map<std::string, specs::RepoDataPackage> conda_packages;
    };

    /**
     * Information dictionary from repodata.
     *
     * Contains channel metadata including base URLs and subdir information.
     */
    struct RepoMetadata
    {
        /** Base URL where packages are stored. */
        std::string base_url;

        /** Base URL where shards are stored. */
        std::string shards_base_url;

        /** Subdirectory (platform) name. */
        std::string subdir;
    };

    /**
     * Shards index dictionary.
     *
     * This is the structure parsed from repodata_shards.msgpack.zst.
     * It maps package names to their shard hash (SHA256).
     */
    struct ShardsIndexDict
    {
        /** Channel information. */
        RepoMetadata info;

        /** Version of the shards index format. */
        std::size_t version = 1;

        /**
         * Map of package names to their shard hash.
         *
         * The hash is stored as raw bytes (32 bytes for SHA256).
         */
        std::map<std::string, std::vector<std::uint8_t>> shards;
    };

    /**
     * Complete repodata dictionary.
     *
     * Combines shard data with repodata metadata.
     */
    struct RepodataDict
    {
        /** Channel information. */
        RepoMetadata info;

        /** Repodata version. */
        std::size_t repodata_version = 2;

        /** Shard dictionary containing packages in both .tar.bz2 and .conda formats. */
        ShardDict shard_dict;
    };

    /**
     * Convert RepodataDict to specs::RepoData.
     *
     * This conversion is used when building repodata for the solver from shards.
     */
    specs::RepoData to_repo_data(const RepodataDict& repodata);

    /**
     * Convert `specs::RepoDataPackage` to `specs::PackageInfo`.
     *
     * This conversion is used when loading packages from shards into the package database.
     * Requires additional metadata (filename, channel, platform, base_url) that is not
     * present in the record itself but needed for PackageInfo.
     *
     * @param record The shard package record to convert
     * @param filename The package filename (e.g., "package-1.0.0-h123_0.tar.bz2")
     * @param channel_id The channel identifier
     * @param platform The platform for this package
     * @param base_url The base URL for constructing package_url
     * @return PackageInfo object with all fields populated
     */
    specs::PackageInfo to_package_info(
        const specs::RepoDataPackage& record,
        const std::string& filename,
        const std::string& channel_id,
        const specs::DynamicPlatform& platform,
        const std::string& base_url
    );

}

#endif
