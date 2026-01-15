// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <set>

#include "mamba/api/channel_loader.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/download_progress_bar.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/shard_index.hpp"
#include "mamba/core/shard_loader.hpp"
#include "mamba/core/shard_traversal.hpp"
#include "mamba/core/shard_types.hpp"
#include "mamba/core/subdir_index.hpp"
#include "mamba/download/downloader.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/specs/version.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/json.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba
{
    namespace
    {
        /**
         * Load a subdir using sharded repodata if available, with support for cross-subdir
         * dependencies.
         *
         * @param ctx Context
         * @param database Database to load into
         * @param subdir SubdirIndexLoader to load
         * @param all_subdirs All subdirs from the same channel (for cross-subdir dependency
         * traversal)
         * @param root_packages Optional root packages for dependency traversal
         * @param loaded_subdirs Output parameter: set of subdir URLs that were loaded (including
         * cross-subdir ones)
         * @return RepoInfo if successful, error otherwise
         */
        auto load_subdir_with_shards(
            const Context& ctx,
            solver::libsolv::Database& database,
            const SubdirIndexLoader& subdir,
            const std::vector<SubdirIndexLoader>& all_subdirs,
            const std::vector<std::string>& root_packages,
            std::set<std::string>& loaded_subdirs
        ) -> expected_t<solver::libsolv::RepoInfo>
        {
            LOG_DEBUG << "Loading subdir with shards: " << subdir.name() << " for root packages: ["
                      << util::join(", ", root_packages) << "]";

            // Check if shards are available
            if (!subdir.metadata().has_up_to_date_shards(ctx.repodata_shards_ttl))
            {
                LOG_DEBUG << "Shards not available for " << subdir.name();
                return make_unexpected(
                    "Shards not available for " + subdir.name(),
                    mamba_error_code::repodata_not_loaded
                );
            }

            LOG_DEBUG << "Shards are available for " << subdir.name() << ", fetching shard index";
            // Fetch shard index
            auto shard_index_result = ShardIndexLoader::fetch_shards_index(
                subdir,
                ctx.subdir_download_params(),
                ctx.authentication_info(),
                ctx.mirrors,
                ctx.download_options(),
                ctx.remote_fetch_params,
                ctx.repodata_shards_ttl
            );

            if (!shard_index_result.has_value())
            {
                LOG_WARNING << "Failed to fetch shard index for " << subdir.name() << ": "
                            << shard_index_result.error().what();
                return make_unexpected(
                    "Failed to fetch shard index",
                    mamba_error_code::repodata_not_loaded
                );
            }

            auto shard_index_opt = shard_index_result.value();
            if (!shard_index_opt.has_value())
            {
                LOG_DEBUG << "Shard index not available for " << subdir.name();
                return make_unexpected(
                    "Shard index not available",
                    mamba_error_code::repodata_not_loaded
                );
            }

            LOG_DEBUG << "Successfully fetched shard index for " << subdir.name() << " with "
                      << shard_index_opt->shards.size() << " package shards";

            // Create Shards objects for all subdirs from the same channel that have shards
            // available This enables cross-subdir dependency traversal (e.g., linux-64 packages
            // depending on noarch)
            std::vector<std::shared_ptr<Shards>> all_shards;
            std::map<std::string, std::shared_ptr<Shards>> shards_by_url;  // Map subdir URL to
                                                                           // Shards object

            // Create Shards object for the current subdir
            // Use subdir.name() as the URL identifier (e.g.,
            // "https://prefix.dev/conda-forge/linux-64") This is used to identify which subdir this
            // Shards object belongs to during traversal
            auto current_shards = std::make_shared<Shards>(
                *shard_index_opt,
                subdir.name(),
                subdir.channel(),
                ctx.authentication_info(),
                ctx.mirrors,
                ctx.remote_fetch_params,
                ctx.repodata_shards_threads
            );
            all_shards.push_back(current_shards);
            shards_by_url[subdir.name()] = current_shards;

            // Create Shards objects for other subdirs from the same channel that have shards
            const std::string current_channel_id = subdir.channel_id();
            for (const auto& other_subdir : all_subdirs)
            {
                // Skip if same subdir, different channel, or no shards available
                if (other_subdir.name() == subdir.name()
                    || other_subdir.channel_id() != current_channel_id
                    || !other_subdir.metadata().has_up_to_date_shards(ctx.repodata_shards_ttl))
                {
                    continue;
                }

                // Fetch shard index for this subdir
                auto other_shard_index_result = ShardIndexLoader::fetch_shards_index(
                    other_subdir,
                    ctx.subdir_download_params(),
                    ctx.authentication_info(),
                    ctx.mirrors,
                    ctx.download_options(),
                    ctx.remote_fetch_params,
                    ctx.repodata_shards_ttl
                );

                if (other_shard_index_result.has_value()
                    && other_shard_index_result.value().has_value())
                {
                    // Use other_subdir.name() as the URL identifier for cross-subdir Shards objects
                    auto other_shards = std::make_shared<Shards>(
                        other_shard_index_result.value().value(),
                        other_subdir.name(),
                        other_subdir.channel(),
                        ctx.authentication_info(),
                        ctx.mirrors,
                        ctx.remote_fetch_params,
                        ctx.repodata_shards_threads
                    );
                    all_shards.push_back(other_shards);
                    shards_by_url[other_subdir.name()] = other_shards;
                    LOG_DEBUG << "Added Shards object for " << other_subdir.name()
                              << " to enable cross-subdir dependency traversal";
                }
            }

            LOG_DEBUG << "Created " << all_shards.size()
                      << " Shards object(s) for cross-subdir traversal, starting dependency traversal";
            // Create RepodataSubset with all Shards objects to enable cross-subdir traversal
            RepodataSubset subset(all_shards);

            // Filter root_packages to only include packages that exist in the current subdir's
            // shard index (dependencies will be discovered across all subdirs during traversal)
            std::vector<std::string> packages_to_traverse;
            if (root_packages.empty())
            {
                // Fetch all packages from the current subdir's shard index
                packages_to_traverse = current_shards->package_names();
                LOG_DEBUG << "No root packages specified, fetching all "
                          << packages_to_traverse.size() << " packages from shard index";
            }
            else
            {
                // Filter root_packages to only those that exist in the current subdir's shard index
                // (not other subdirs - those will be discovered as dependencies during traversal)
                for (const auto& pkg : root_packages)
                {
                    if (current_shards->contains(pkg))
                    {
                        packages_to_traverse.push_back(pkg);
                    }
                }
                if (packages_to_traverse.empty())
                {
                    LOG_DEBUG << "None of the root packages [" << util::join(", ", root_packages)
                              << "] exist in " << subdir.name()
                              << " shard index, skipping traversal";
                    return make_unexpected(
                        "No root packages found in shard index for " + subdir.name(),
                        mamba_error_code::repodata_not_loaded
                    );
                }
                LOG_DEBUG << "Filtered root packages for " << subdir.name() << ": ["
                          << util::join(", ", root_packages) << "] -> ["
                          << util::join(", ", packages_to_traverse) << "]";
            }

            // Track if we need to add pip separately (not restricted to current subdir)
            // We add pip when add_pip_as_python_dependency is enabled, as Python (which may be
            // discovered as a dependency) will require pip
            bool need_pip = ctx.add_pip_as_python_dependency;

            LOG_DEBUG << "Starting dependency traversal for " << packages_to_traverse.size()
                      << " root package(s): [" << util::join(", ", packages_to_traverse) << "]";
            // Pass current_shards to restrict root node creation to the current subdir
            // (dependencies will still be discovered across all subdirs)
            auto traversal_result = subset.reachable(packages_to_traverse, "pipelined", current_shards);

            // If pip is needed, add it separately without restricting to current subdir
            // This ensures pip is discovered from noarch (where newer versions are)
            if (need_pip && traversal_result.has_value())
            {
                std::vector<std::string> pip_packages = { "pip" };
                // Pass nullptr to not restrict to current subdir - let it be discovered from all
                // subdirs
                auto pip_traversal_result = subset.reachable(pip_packages, "pipelined", nullptr);
                if (!pip_traversal_result.has_value())
                {
                    LOG_WARNING << "Failed to traverse 'pip' from all subdirs: "
                                << pip_traversal_result.error().what();
                }
            }
            if (!traversal_result.has_value())
            {
                LOG_WARNING << "Failed to traverse dependencies for " << subdir.name() << ": "
                            << traversal_result.error().what();
                return make_unexpected(
                    "Failed to traverse dependencies",
                    mamba_error_code::repodata_not_loaded
                );
            }

            LOG_DEBUG << "Dependency traversal completed for " << subdir.name();

            // Collect all PackageInfo objects from visited shards, grouped by subdir
            // We need to group by subdir because each subdir needs its own repo in the database
            std::map<std::string, std::vector<specs::PackageInfo>> packages_by_subdir;

            // Get all visited packages from the traversal nodes, grouped by channel URL
            std::map<std::string, std::set<std::string>> visited_packages_by_url;
            for (const auto& [node_id, node] : subset.nodes())
            {
                if (node.visited)
                {
                    visited_packages_by_url[node_id.channel].insert(node_id.package);
                }
            }

            LOG_DEBUG << "Converting visited package shard(s) to PackageInfo across "
                      << visited_packages_by_url.size() << " subdir(s)";
            // Load shards for all visited packages and convert to PackageInfo, grouped by subdir
            for (const auto& [subdir_url, visited_packages] : visited_packages_by_url)
            {
                auto shards_it = shards_by_url.find(subdir_url);
                if (shards_it == shards_by_url.end())
                {
                    LOG_WARNING << "No Shards object found for subdir URL: " << subdir_url;
                    continue;
                }
                auto& shards_for_subdir = shards_it->second;

                // Find the corresponding SubdirIndexLoader to get platform and channel info
                const SubdirIndexLoader* subdir_loader = nullptr;
                for (const auto& sd : all_subdirs)
                {
                    if (sd.name() == subdir_url && sd.channel_id() == current_channel_id)
                    {
                        subdir_loader = &sd;
                        break;
                    }
                }
                if (subdir_loader == nullptr)
                {
                    LOG_WARNING << "No SubdirIndexLoader found for subdir URL: " << subdir_url;
                    continue;
                }

                std::string base_url = subdir_loader->name();
                if (!base_url.empty() && base_url.back() == '/')
                {
                    base_url.pop_back();
                }
                if (base_url.empty())
                {
                    LOG_ERROR << "base_url is empty for " << subdir_loader->channel_id() << "/"
                              << subdir_loader->platform();
                    continue;
                }
                const specs::DynamicPlatform& platform = subdir_loader->platform();
                const std::string channel_id = subdir_loader->channel_id();

                LOG_DEBUG << "Processing " << visited_packages.size()
                          << " visited package(s) from subdir " << subdir_url;

                for (const auto& package_name : visited_packages)
                {
                    LOG_DEBUG << "Fetching shard for visited package '" << package_name
                              << "' from subdir " << subdir_url << " to convert to PackageInfo";
                    // Fetch the shard for this package
                    auto shard_result = shards_for_subdir->fetch_shard(package_name);
                    if (!shard_result.has_value())
                    {
                        LOG_WARNING << "Failed to fetch shard for package " << package_name
                                    << " from " << subdir_url << ": " << shard_result.error().what();
                        continue;
                    }

                    const auto& shard = shard_result.value();

                    // Convert packages from shard to PackageInfo
                    for (const auto& [filename, record] : shard.packages)
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
                            LOG_DEBUG << "Set sha256 for package '" << pkg_info.name << "' ("
                                      << filename << "): " << *record.sha256;
                        }
                        else
                        {
                            LOG_DEBUG << "No sha256 found for package '" << pkg_info.name << "' ("
                                      << filename << ")";
                        }
                        if (record.md5)
                        {
                            pkg_info.md5 = *record.md5;
                            LOG_DEBUG << "Set md5 for package '" << pkg_info.name << "' ("
                                      << filename << "): " << *record.md5;
                        }
                        else
                        {
                            LOG_DEBUG << "No md5 found for package '" << pkg_info.name << "' ("
                                      << filename << ")";
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

                        packages_by_subdir[subdir_url].push_back(std::move(pkg_info));
                    }

                    // Convert conda packages from shard to PackageInfo
                    for (const auto& [filename, record] : shard.conda_packages)
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
                            LOG_DEBUG << "Set sha256 for conda package '" << pkg_info.name << "' ("
                                      << filename << "): " << *record.sha256;
                        }
                        else
                        {
                            LOG_DEBUG << "No sha256 found for conda package '" << pkg_info.name
                                      << "' (" << filename << ")";
                        }
                        if (record.md5)
                        {
                            pkg_info.md5 = *record.md5;
                            LOG_DEBUG << "Set md5 for conda package '" << pkg_info.name << "' ("
                                      << filename << "): " << *record.md5;
                        }
                        else
                        {
                            LOG_DEBUG << "No md5 found for conda package '" << pkg_info.name
                                      << "' (" << filename << ")";
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

                        packages_by_subdir[subdir_url].push_back(std::move(pkg_info));
                    }
                }  // End of loop over visited packages for this subdir
            }  // End of loop over subdirs

            // Collect packages for the current subdir (the one we're loading)
            auto& package_infos = packages_by_subdir[subdir.name()];

            LOG_DEBUG << "Collected " << package_infos.size()
                      << " PackageInfo object(s) from shards for " << subdir.name();
            if (package_infos.empty())
            {
                LOG_DEBUG << "No packages found in shards for " << subdir.name()
                          << " after traversal and conversion";
                return make_unexpected(
                    "No packages found in shards",
                    mamba_error_code::repodata_not_loaded
                );
            }

            // Sort packages by name, then by version (descending), then by build number
            // (descending) This ensures that when libsolv processes packages, it sees them in the
            // correct order (highest version/build first), which helps it select the latest version
            std::sort(
                package_infos.begin(),
                package_infos.end(),
                [](const specs::PackageInfo& a, const specs::PackageInfo& b)
                {
                    // First compare by package name
                    if (a.name != b.name)
                    {
                        return a.name < b.name;
                    }

                    // Then compare by version (descending - highest first)
                    auto version_a = specs::Version::parse(a.version);
                    auto version_b = specs::Version::parse(b.version);

                    if (version_a.has_value() && version_b.has_value())
                    {
                        if (version_a.value() != version_b.value())
                        {
                            return version_b.value() < version_a.value();  // Descending order
                        }
                    }
                    else if (version_a.has_value() || version_b.has_value())
                    {
                        // If only one can be parsed, prefer the parsed one
                        return !version_a.has_value();
                    }
                    else
                    {
                        // Fallback to string comparison if parsing fails
                        if (a.version != b.version)
                        {
                            return b.version < a.version;  // Descending order
                        }
                    }

                    // Finally compare by build number (descending - highest first)
                    if (a.build_number != b.build_number)
                    {
                        return b.build_number < a.build_number;  // Descending order
                    }

                    // If everything else is equal, compare by build string
                    return b.build_string < a.build_string;  // Descending order
                }
            );

            // Add packages directly to database
            // Note: add_repo_from_packages always succeeds (returns RepoInfo, not expected_t)
            // Once we call this, the repo is added and we MUST NOT fall back to avoid double-add
            const auto add_pip = static_cast<solver::libsolv::PipAsPythonDependency>(
                ctx.add_pip_as_python_dependency
            );

            // CRITICAL: Do NOT set metadata for repos created from packages.
            // Setting metadata (set_url, set_etag, etc.) even BEFORE internalize() appears to
            // cause memory corruption. The metadata is only used for caching solv files, which
            // we skip for sharded repos anyway. Using the standard add_repo_from_packages
            // without metadata.
            // Use channel_id as repo name (same as traditional loading)
            auto repo_info = database.add_repo_from_packages(package_infos, subdir.channel_id(), add_pip);

            // Also add packages from other subdirs that were discovered during traversal
            // (e.g., noarch packages discovered when traversing linux-64 dependencies)
            // Note: These subdirs will be skipped in the main loop to avoid double-loading
            for (const auto& [other_subdir_url, other_packages] : packages_by_subdir)
            {
                if (other_subdir_url == subdir.name())
                {
                    continue;  // Already added above
                }

                if (other_packages.empty())
                {
                    continue;
                }

                // Find the SubdirIndexLoader for this subdir to get channel_id
                const SubdirIndexLoader* other_subdir_loader = nullptr;
                for (const auto& sd : all_subdirs)
                {
                    if (sd.name() == other_subdir_url && sd.channel_id() == current_channel_id)
                    {
                        other_subdir_loader = &sd;
                        break;
                    }
                }
                if (other_subdir_loader == nullptr)
                {
                    LOG_WARNING << "No SubdirIndexLoader found for cross-subdir packages from "
                                << other_subdir_url;
                    continue;
                }

                // Sort cross-subdir packages by version (descending) as well
                auto sorted_other_packages = other_packages;
                std::sort(
                    sorted_other_packages.begin(),
                    sorted_other_packages.end(),
                    [](const specs::PackageInfo& a, const specs::PackageInfo& b)
                    {
                        // First compare by package name
                        if (a.name != b.name)
                        {
                            return a.name < b.name;
                        }

                        // Then compare by version (descending - highest first)
                        auto version_a = specs::Version::parse(a.version);
                        auto version_b = specs::Version::parse(b.version);

                        if (version_a.has_value() && version_b.has_value())
                        {
                            if (version_a.value() != version_b.value())
                            {
                                return version_b.value() < version_a.value();  // Descending order
                            }
                        }
                        else if (version_a.has_value() || version_b.has_value())
                        {
                            // If only one can be parsed, prefer the parsed one
                            return !version_a.has_value();
                        }
                        else
                        {
                            // Fallback to string comparison if parsing fails
                            if (a.version != b.version)
                            {
                                return b.version < a.version;  // Descending order
                            }
                        }

                        // Finally compare by build number (descending - highest first)
                        if (a.build_number != b.build_number)
                        {
                            return b.build_number < a.build_number;  // Descending order
                        }

                        // If everything else is equal, compare by build string
                        return b.build_string < a.build_string;  // Descending order
                    }
                );

                LOG_DEBUG << "Adding " << sorted_other_packages.size()
                          << " cross-subdir package(s) from " << other_subdir_url
                          << " discovered during traversal";
                // Add packages from this other subdir to the database
                // Use the same channel_id since they're from the same channel
                // Note: libsolv handles multiple repos with the same name internally
                database.add_repo_from_packages(
                    sorted_other_packages,
                    other_subdir_loader->channel_id(),
                    add_pip
                );
                // Mark this subdir as loaded to prevent double-loading in main loop
                loaded_subdirs.insert(other_subdir_url);
            }

            // Mark the current subdir as loaded
            loaded_subdirs.insert(subdir.name());

            // Write solv file if not on Windows
            // If this fails, we still return success since the repo is already added
            // Failure to write solv file should not cause fallback (which would double-add)
            // CRITICAL: For repos created from packages, metadata is already set before
            // internalize(). Calling write_solv which tries to read metadata (even just to
            // check if it's set) can cause memory corruption. We skip writing the solv file
            // for now - it's not critical for functionality, just for caching.
            // TODO: Fix write_solv to safely handle already-internalized repos with metadata set
            // if (!util::on_win)
            // {
            //     const auto expected_cache_origin = solver::libsolv::RepodataOrigin{
            //         /* .url= */ repo_url,
            //         /* .etag= */ subdir.metadata().etag(),
            //         /* .mod= */ subdir.metadata().last_modified(),
            //     };
            //
            //     auto result = database.native_serialize_repo(
            //         repo_info,
            //         subdir.writable_libsolv_cache_path(),
            //         expected_cache_origin
            //     );
            //     if (!result)
            //     {
            //         LOG_WARNING << R"(Fail to write native serialization to file ")"
            //                     << subdir.writable_libsolv_cache_path() << R"(" for repo ")"
            //                     << subdir.name() << ": " << std::move(result).error().what();
            //     }
            // }

            return repo_info;
        }

        auto create_repo_from_pkgs_dir(
            const Context& ctx,
            ChannelContext& channel_context,
            solver::libsolv::Database& database,
            const fs::u8path& pkgs_dir
        ) -> solver::libsolv::RepoInfo
        {
            if (!fs::exists(pkgs_dir))
            {
                // TODO : us tl::expected mechanism
                throw std::runtime_error("Specified pkgs_dir does not exist\n");
            }
            auto sprefix_data = PrefixData::create(pkgs_dir, channel_context);
            if (!sprefix_data)
            {
                throw std::runtime_error("Specified pkgs_dir does not exist\n");
            }
            PrefixData& prefix_data = sprefix_data.value();
            for (const auto& entry : fs::directory_iterator(pkgs_dir))
            {
                fs::u8path repodata_record_json = entry.path() / "info" / "repodata_record.json";
                if (!fs::exists(repodata_record_json))
                {
                    continue;
                }
                prefix_data.load_single_record(repodata_record_json);
            }
            return load_installed_packages_in_database(ctx, database, prefix_data);
        }

        void create_subdirs(
            Context& ctx,
            ChannelContext& channel_context,
            const specs::Channel& channel,
            MultiPackageCache& package_caches,
            std::vector<SubdirIndexLoader>& subdirs,
            std::vector<mamba_error>& error_list,
            std::vector<solver::libsolv::Priorities>& priorities,
            int& max_prio,
            specs::CondaURL& prev_channel_url
        )
        {
            static bool has_shown_anaconda_channel_warning = false;
            for (const auto& platform : channel.platforms())
            {
                auto show_warning = ctx.show_anaconda_channel_warnings;
                auto channel_name = channel.platform_url(platform).host();
                if (channel_name == "repo.anaconda.com" && show_warning
                    && !has_shown_anaconda_channel_warning)
                {
                    has_shown_anaconda_channel_warning = true;
                    LOG_WARNING << "'" << channel_name
                                << "', a commercial channel hosted by Anaconda.com, is used.\n";
                    LOG_WARNING << "Please make sure you understand Anaconda Terms of Services.\n";
                    LOG_WARNING << "See: https://legal.anaconda.com/policies/en/";
                }

                auto subdir_params = ctx.subdir_params();
                subdir_params.repodata_force_use_zst = channel_context.has_zst(channel);
                auto sdires = SubdirIndexLoader::create(
                    subdir_params,
                    channel,
                    platform,
                    package_caches,
                    "repodata.json"
                );
                if (!sdires.has_value())
                {
                    error_list.push_back(std::move(sdires).error());
                    continue;
                }
                auto sdir = std::move(sdires).value();
                if (sdir.valid_cache_found())
                {
                    Console::stream() << fmt::format("{:<50} {:>20}", sdir.name(), "Using cache");
                }

                subdirs.push_back(std::move(sdir));
                if (ctx.channel_priority == ChannelPriority::Disabled)
                {
                    priorities.push_back({ /* .priority= */ 0, /* .subpriority= */ 0 });
                }
                else
                {
                    // Consider 'flexible' and 'strict' the same way
                    if (channel.url() != prev_channel_url)
                    {
                        max_prio--;
                        prev_channel_url = channel.url();
                    }
                    priorities.push_back({ /* .priority= */ max_prio, /* .subpriority= */ 0 });
                }
            }
        }

        void create_mirrors(const specs::Channel& channel, download::mirror_map& mirrors)
        {
            if (!mirrors.has_mirrors(channel.id()))
            {
                for (const specs::CondaURL& url : channel.mirror_urls())
                {
                    mirrors.add_unique_mirror(
                        channel.id(),
                        download::make_mirror(url.str(specs::CondaURL::Credentials::Show))
                    );
                }
            }
        }

        auto load_channels_impl(
            Context& ctx,
            ChannelContext& channel_context,
            solver::libsolv::Database& database,
            MultiPackageCache& package_caches,
            bool is_retry,
            const std::vector<std::string>& root_packages = {}
        ) -> expected_t<void, mamba_aggregated_error>
        {
            // When sharded repodata is enabled, automatically disable safety_checks to avoid
            // warnings about missing md5/sha256 in repodata_record.json files.
            // This is necessary because packages loaded from shards don't include md5/sha256
            // hashes in their repodata_record.json files (they are parsed from shard data but
            // not written to the cached repodata_record.json), which would otherwise trigger
            // validation warnings.
            if (ctx.repodata_use_shards
                && ctx.validation_params.safety_checks != VerificationLevel::Disabled)
            {
                LOG_DEBUG << "Sharded repodata is enabled, automatically disabling safety_checks "
                          << "to avoid warnings about missing md5/sha256 in repodata_record.json";
                ctx.validation_params.safety_checks = VerificationLevel::Disabled;
            }

            std::vector<SubdirIndexLoader> subdirs;

            std::vector<solver::libsolv::Priorities> priorities;
            int max_prio = static_cast<int>(ctx.channels.size());
            auto prev_channel_url = specs::CondaURL();

            Console::instance().init_progress_bar_manager(ProgressBarMode::multi);

            std::vector<mamba_error> error_list;

            for (const auto& mirror : ctx.mirrored_channels)
            {
                for (auto channel : channel_context.make_channel(mirror.first, mirror.second))
                {
                    create_mirrors(channel, ctx.mirrors);
                    create_subdirs(
                        ctx,
                        channel_context,
                        channel,
                        package_caches,
                        subdirs,
                        error_list,
                        priorities,
                        max_prio,
                        prev_channel_url
                    );
                }
            }

            auto packages = std::vector<specs::PackageInfo>();

            for (const auto& location : ctx.channels)
            {
                // TODO: C++20, replace with contains
                if (ctx.mirrored_channels.find(location) == ctx.mirrored_channels.end())
                {
                    for (auto channel : channel_context.make_channel(location))
                    {
                        if (channel.is_package())
                        {
                            auto pkg_info = specs::PackageInfo::from_url(channel.url().str())
                                                .or_else([](specs::ParseError&& err)
                                                         { throw std::move(err); })
                                                .value();
                            packages.push_back(pkg_info);
                            continue;
                        }

                        create_mirrors(channel, ctx.mirrors);
                        create_subdirs(
                            ctx,
                            channel_context,
                            channel,
                            package_caches,
                            subdirs,
                            error_list,
                            priorities,
                            max_prio,
                            prev_channel_url
                        );
                    }
                }
            }

            if (!packages.empty())
            {
                database.add_repo_from_packages(packages, "packages");
            }

            // Download check requests first (to check metadata and shard availability)
            auto check_requests = SubdirIndexLoader::build_all_check_requests(
                subdirs.begin(),
                subdirs.end(),
                ctx.subdir_download_params()
            );

            download::Monitor* check_monitor_ptr = nullptr;
            SubdirIndexMonitor check_monitor({ true, true });
            if (SubdirIndexMonitor::can_monitor(ctx))
            {
                check_monitor_ptr = &check_monitor;
            }

            expected_t<void> check_res;
            try
            {
                auto check_results = download::download(
                    std::move(check_requests),
                    ctx.mirrors,
                    ctx.remote_fetch_params,
                    ctx.authentication_info(),
                    ctx.download_options(),
                    check_monitor_ptr
                );
                // Allow to continue if failed checks (some subdirs might not exist)
                for (auto& result : check_results)
                {
                    if (!result.has_value())
                    {
                        LOG_WARNING << "Failed to check subdir: " << result.error().message;
                    }
                }
                check_res = {};
            }
            catch (const std::runtime_error& e)
            {
                check_res = make_unexpected(e.what(), mamba_error_code::repodata_not_loaded);
            }

            // Allow to continue if failed checks, unless interrupted
            constexpr auto is_interrupted = [](const auto& e)
            { return e.error_code() == mamba_error_code::user_interrupted; };

            if (!check_res.has_value() && check_res.map_error(is_interrupted).error())
            {
                mamba_error error = check_res.error();
                error_list.push_back(std::move(error));
                return tl::unexpected(mamba_aggregated_error(std::move(error_list)));
            }

            // Filter subdirs that need index downloads (repodata.json)
            // Skip index downloads for subdirs with shards available when repodata_use_shards is
            // true and root_packages is not empty
            std::vector<SubdirIndexLoader*> subdirs_needing_index;
            for (auto& subdir : subdirs)
            {
                bool needs_index = true;
                if (ctx.repodata_use_shards && !root_packages.empty())
                {
                    // Check if shards are available for this subdir
                    if (subdir.metadata().has_up_to_date_shards(ctx.repodata_shards_ttl))
                    {
                        // Skip downloading repodata.json if shards are available
                        needs_index = false;
                        LOG_DEBUG << "Skipping repodata.json download for " << subdir.name()
                                  << " (using sharded repodata)";
                    }
                }
                if (needs_index)
                {
                    subdirs_needing_index.push_back(&subdir);
                }
            }

            // Download index requests only for subdirs that need them
            if (!subdirs_needing_index.empty())
            {
                auto index_requests = SubdirIndexLoader::build_all_index_requests(
                    subdirs_needing_index.begin(),
                    subdirs_needing_index.end(),
                    ctx.subdir_download_params()
                );

                download::Monitor* index_monitor_ptr = nullptr;
                SubdirIndexMonitor index_monitor;
                if (SubdirIndexMonitor::can_monitor(ctx))
                {
                    index_monitor_ptr = &index_monitor;
                }

                expected_t<void> index_res;
                try
                {
                    auto index_results = download::download(
                        std::move(index_requests),
                        ctx.mirrors,
                        ctx.remote_fetch_params,
                        ctx.authentication_info(),
                        ctx.download_options(),
                        index_monitor_ptr
                    );
                    // Allow to continue if some downloads fail (some subdirs might not exist)
                    for (auto& result : index_results)
                    {
                        if (!result.has_value())
                        {
                            LOG_WARNING << "Failed to download subdir index: "
                                        << result.error().message;
                        }
                    }
                    index_res = {};
                }
                catch (const std::runtime_error& e)
                {
                    index_res = make_unexpected(e.what(), mamba_error_code::repodata_not_loaded);
                }

                if (!index_res)
                {
                    mamba_error error = index_res.error();
                    mamba_error_code ec = error.error_code();
                    error_list.push_back(std::move(error));
                    if (ec == mamba_error_code::user_interrupted)
                    {
                        return tl::unexpected(mamba_aggregated_error(std::move(error_list)));
                    }
                }
            }

            if (ctx.offline)
            {
                LOG_INFO << "Creating repo from pkgs_dir for offline";
                for (const auto& c : ctx.pkgs_dirs)
                {
                    create_repo_from_pkgs_dir(ctx, channel_context, database, c);
                }
            }
            std::string prev_channel;
            bool loading_failed = false;
            // Track which subdirs have already been loaded with shards to avoid double-loading
            // when cross-subdir dependencies are discovered
            std::set<std::string> loaded_subdirs_with_shards;
            for (std::size_t i = 0; i < subdirs.size(); ++i)
            {
                auto& subdir = subdirs[i];

                // Check if we can use shards (even if traditional cache is invalid)
                bool can_use_shards = ctx.repodata_use_shards
                                      && subdir.metadata().has_up_to_date_shards(ctx.repodata_shards_ttl)
                                      && !root_packages.empty();

                // If cache is invalid and we can't use shards, skip this subdir
                if (!subdir.valid_cache_found() && !can_use_shards)
                {
                    if (!ctx.offline && subdir.is_noarch())
                    {
                        error_list.push_back(mamba_error(
                            "Subdir " + subdir.name() + " not loaded!",
                            mamba_error_code::subdirdata_not_loaded
                        ));
                    }
                    continue;
                }

                // Skip if this subdir was already loaded as a cross-subdir dependency
                if (loaded_subdirs_with_shards.find(subdir.name()) != loaded_subdirs_with_shards.end())
                {
                    LOG_DEBUG << "Skipping " << subdir.name()
                              << " - already loaded as cross-subdir dependency";
                    continue;
                }

                // Try sharded repodata first if available and enabled
                expected_t<solver::libsolv::RepoInfo> result = make_unexpected(
                    "Not attempted",
                    mamba_error_code::repodata_not_loaded
                );

                if (can_use_shards)
                {
                    result = load_subdir_with_shards(
                        ctx,
                        database,
                        subdir,
                        subdirs,
                        root_packages,
                        loaded_subdirs_with_shards
                    );

                    // If sharded loading fails, try to fall back to traditional repodata
                    if (!result.has_value())
                    {
                        std::string error_msg = result.error().what();
                        bool is_no_root_packages_error = error_msg.find(
                                                             "No root packages found in shard index"
                                                         )
                                                         != std::string::npos;

                        if (is_no_root_packages_error)
                        {
                            LOG_DEBUG
                                << "Skipping " << subdir.name()
                                << " (none of the root packages exist in this subdir's shards)";
                            // Skip this subdir - don't load it at all
                            continue;
                        }
                        else
                        {
                            LOG_WARNING << "Sharded repodata loading failed for " << subdir.name()
                                        << ": " << error_msg
                                        << " (falling back to traditional repodata)";
                            // Fall back to traditional repodata
                            if (subdir.valid_cache_found())
                            {
                                result = load_subdir_in_database(ctx, database, subdir);
                            }
                            else
                            {
                                // Try to download repodata.json for fallback
                                LOG_DEBUG << "Attempting to download repodata.json for "
                                          << subdir.name() << " as fallback from shard failure";
                                std::vector<SubdirIndexLoader*> fallback_subdirs = { &subdir };
                                auto index_requests = SubdirIndexLoader::build_all_index_requests(
                                    fallback_subdirs.begin(),
                                    fallback_subdirs.end(),
                                    ctx.subdir_download_params()
                                );
                                if (!index_requests.empty())
                                {
                                    try
                                    {
                                        auto index_results = download::download(
                                            std::move(index_requests),
                                            ctx.mirrors,
                                            ctx.remote_fetch_params,
                                            ctx.authentication_info(),
                                            ctx.download_options(),
                                            nullptr  // monitor
                                        );
                                        // Check if download succeeded
                                        bool download_succeeded = false;
                                        for (auto& index_result : index_results)
                                        {
                                            if (index_result.has_value())
                                            {
                                                download_succeeded = true;
                                                break;
                                            }
                                        }
                                        if (download_succeeded && subdir.valid_cache_found())
                                        {
                                            result = load_subdir_in_database(ctx, database, subdir);
                                        }
                                        else
                                        {
                                            LOG_WARNING << "Failed to download repodata.json for "
                                                        << subdir.name() << " as fallback";
                                            continue;
                                        }
                                    }
                                    catch (const std::exception& e)
                                    {
                                        LOG_WARNING
                                            << "Exception while downloading repodata.json for "
                                            << subdir.name() << " as fallback: " << e.what();
                                        continue;
                                    }
                                }
                                else
                                {
                                    LOG_WARNING << "Cannot fall back to traditional repodata for "
                                                << subdir.name()
                                                << " - no cache found and cannot download";
                                    continue;
                                }
                            }
                        }
                    }
                }
                else
                {
                    // Use traditional repodata.json (only if cache is valid)
                    if (subdir.valid_cache_found())
                    {
                        result = load_subdir_in_database(ctx, database, subdir);
                    }
                    else
                    {
                        LOG_ERROR << "Cannot load subdir " << subdir.name()
                                  << " - no cache found and shards not available";
                        // Continue to error handling below
                    }
                }

                if (result)
                {
                    database.set_repo_priority(std::move(result).value(), priorities[i]);
                }
                else if (is_retry)
                {
                    std::stringstream ss;
                    ss << "Could not load repodata.json for " << subdir.name() << " after retry."
                       << "Please check repodata source. Exiting." << std::endl;
                    error_list.push_back(mamba_error(ss.str(), mamba_error_code::repodata_not_loaded));
                }
                else
                {
                    LOG_WARNING << "Could not load repodata.json for " << subdir.name()
                                << ". Deleting cache, and retrying.";
                    subdir.clear_valid_cache_files();
                    loading_failed = true;
                }
            }

            if (loading_failed)
            {
                if (!ctx.offline && !is_retry)
                {
                    LOG_WARNING << "Encountered malformed repodata.json cache. Redownloading.";
                    bool retry = true;
                    return load_channels_impl(ctx, channel_context, database, package_caches, retry, {});
                }
                error_list.emplace_back(
                    "Could not load repodata. Cache corrupted?",
                    mamba_error_code::repodata_not_loaded
                );
            }
            using return_type = expected_t<void, mamba_aggregated_error>;
            return error_list.empty() ? return_type()
                                      : return_type(make_unexpected(std::move(error_list)));
        }
    }

    auto load_channels(
        Context& ctx,
        ChannelContext& channel_context,
        solver::libsolv::Database& database,
        MultiPackageCache& package_caches,
        const std::vector<std::string>& root_packages
    ) -> expected_t<void, mamba_aggregated_error>
    {
        bool retry = false;
        return load_channels_impl(ctx, channel_context, database, package_caches, retry, root_packages);
    }

    void init_channels(Context& context, ChannelContext& channel_context)
    {
        for (const auto& mirror : context.mirrored_channels)
        {
            for (auto channel : channel_context.make_channel(mirror.first, mirror.second))
            {
                create_mirrors(channel, context.mirrors);
            }
        }

        for (const auto& location : context.channels)
        {
            // TODO: C++20, replace with contains
            if (context.mirrored_channels.find(location) == context.mirrored_channels.end())
            {
                for (auto channel : channel_context.make_channel(location))
                {
                    create_mirrors(channel, context.mirrors);
                }
            }
        }
    }

    void init_channels_from_package_urls(
        Context& context,
        ChannelContext& channel_context,
        const std::vector<std::string>& specs
    )
    {
        for (const auto& spec : specs)
        {
            auto pkg_info = specs::PackageInfo::from_url(spec)
                                .or_else([](specs::ParseError&& err) { throw std::move(err); })
                                .value();
            for (auto channel : channel_context.make_channel(pkg_info.channel))
            {
                create_mirrors(channel, context.mirrors);
            }
        }
    }

}
