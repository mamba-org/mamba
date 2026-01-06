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

#include "mamba/specs/repo_data.hpp"

namespace mamba
{
    /**
     * Package record dictionary for shard data.
     *
     * This is a simplified representation of package metadata used in shards.
     * It contains only the fields needed for dependency traversal.
     */
    struct ShardPackageRecord
    {
        std::string name;
        std::string version;
        std::string build;
        std::size_t build_number = 0;
        std::optional<std::string> sha256;
        std::optional<std::string> md5;
        std::vector<std::string> depends;
        std::vector<std::string> constrains;
        std::optional<std::string> noarch;
    };

    /**
     * A shard dictionary containing packages for a single package name.
     *
     * Maps to the Python ShardDict type. Contains all versions of a package
     * in both .tar.bz2 and .conda formats.
     */
    struct ShardDict
    {
        /** Packages in .tar.bz2 format, keyed by filename. */
        std::map<std::string, ShardPackageRecord> packages;

        /** Packages in .conda format, keyed by filename. */
        std::map<std::string, ShardPackageRecord> conda_packages;
    };

    /**
     * Information dictionary from repodata.
     *
     * Contains channel metadata including base URLs and subdir information.
     */
    struct RepodataInfoDict
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
        RepodataInfoDict info;

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
        RepodataInfoDict info;

        /** Repodata version. */
        std::size_t repodata_version = 2;

        /** Packages in .tar.bz2 format, keyed by filename. */
        std::map<std::string, ShardPackageRecord> packages;

        /** Packages in .conda format, keyed by filename. */
        std::map<std::string, ShardPackageRecord> conda_packages;
    };

    /**
     * Convert ShardPackageRecord to specs::RepoDataPackage.
     *
     * This conversion is used when building repodata for the solver.
     */
    specs::RepoDataPackage to_repo_data_package(const ShardPackageRecord& record);

    /**
     * Convert specs::RepoDataPackage to ShardPackageRecord.
     *
     * This conversion is used when treating monolithic repodata as shards.
     */
    ShardPackageRecord from_repo_data_package(const specs::RepoDataPackage& record);

    /**
     * Convert RepodataDict to specs::RepoData.
     *
     * This conversion is used when building repodata for the solver from shards.
     */
    specs::RepoData to_repo_data(const RepodataDict& repodata);

}

#endif
