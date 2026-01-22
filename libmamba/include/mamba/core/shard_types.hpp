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
     * Package record dictionary for shard data.
     *
     * This is a simplified representation of package metadata used in shards.
     * It exists separately from other package types for several reasons:
     *
     * **Comparison with specs::RepoDataPackage:**
     * - Uses primitive types (string for version, string for noarch) instead of
     *   complex types (Version object, NoArchType enum), making direct msgpack
     *   deserialization faster and more straightforward.
     * - Contains only fields needed for dependency traversal, reducing memory usage
     *   when processing many shards.
     * - Conversion to RepoDataPackage happens lazily when building repodata for
     *   the solver, deferring parsing costs until actually needed.
     *
     * **Comparison with specs::PackageInfo:**
     * - PackageInfo is the runtime representation used for installed packages,
     *   transactions, and queries. It uses string for version (like ShardPackageRecord)
     *   but NoArchType enum (like RepoDataPackage), and includes runtime-specific
     *   fields like channel, package_url, platform, filename, signatures, etc.
     * - ShardPackageRecord is purely for parsing msgpack shards and contains only
     *   the minimal fields needed for dependency traversal.
     * - PackageInfo is created from RepoDataPackage when packages are added to the
     *   solver database, not directly from ShardPackageRecord.
     *
     * **Key design decisions:**
     *
     * 1. **Simpler msgpack parsing**: The msgpack format from Python shards uses simple
     *    types that map directly to primitives, avoiding complex type parsing during
     *    deserialization.
     *
     * 2. **Minimal storage**: Only fields needed for dependency traversal (name, version,
     *    build, dependencies, constraints). Fields like license, timestamp, track_features
     *    are not needed during traversal.
     *
     * 3. **Lazy conversion**: Conversion to specs::RepoDataPackage happens only when
     *    building repodata for the solver (via to_repo_data_package()), deferring
     *    Version/NoArchType parsing costs until actually needed.
     *
     * 4. **Flexible msgpack handling**: Custom parsing handles various msgpack types
     *    for sha256/md5 (strings, bytes, EXT types), easier with a dedicated structure.
     *
     * This structure supports all fields defined in the shard format specification.
     * See https://conda.org/learn/ceps/cep-0016 for the complete shard format specification.
     *
     * @see specs::RepoDataPackage The canonical package record type used for repodata.json
     * @see specs::PackageInfo The runtime package representation used throughout the codebase
     * @see to_repo_data_package() Conversion function to RepoDataPackage
     * @see from_repo_data_package() Conversion function from RepoDataPackage
     * @see https://conda.org/learn/ceps/cep-0016 CEP 16 - Sharded Repodata specification
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
        std::size_t size = 0;
        std::optional<std::string> license;
        std::optional<std::string> license_family;
        std::optional<std::string> subdir;
        std::optional<std::size_t> timestamp;
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

    /**
     * Convert ShardPackageRecord to specs::PackageInfo.
     *
     * This conversion is used when loading packages from shards into the package database.
     * Requires additional metadata (filename, channel, platform, base_url) that is not
     * present in ShardPackageRecord but needed for PackageInfo.
     *
     * @param record The shard package record to convert
     * @param filename The package filename (e.g., "package-1.0.0-h123_0.tar.bz2")
     * @param channel_id The channel identifier
     * @param platform The platform for this package
     * @param base_url The base URL for constructing package_url
     * @return PackageInfo object with all fields populated
     */
    specs::PackageInfo to_package_info(
        const ShardPackageRecord& record,
        const std::string& filename,
        const std::string& channel_id,
        const specs::DynamicPlatform& platform,
        const std::string& base_url
    );

}

#endif
