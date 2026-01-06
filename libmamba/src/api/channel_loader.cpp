// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>

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
#include "mamba/util/environment.hpp"
#include "mamba/util/json.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba
{
    namespace
    {
        /**
         * Load a subdir using sharded repodata if available.
         *
         * @param ctx Context
         * @param database Database to load into
         * @param subdir SubdirIndexLoader to load
         * @param root_packages Optional root packages for dependency traversal
         * @return RepoInfo if successful, error otherwise
         */
        auto load_subdir_with_shards(
            const Context& ctx,
            solver::libsolv::Database& database,
            const SubdirIndexLoader& subdir,
            const std::vector<std::string>& root_packages = {}
        ) -> expected_t<solver::libsolv::RepoInfo>
        {
            // Check if shards are available
            if (!subdir.metadata().has_up_to_date_shards())
            {
                return make_unexpected(
                    "Shards not available for " + subdir.name(),
                    mamba_error_code::repodata_not_loaded
                );
            }

            // Fetch shard index
            auto shard_index_result = ShardIndexLoader::fetch_shards_index(
                subdir,
                ctx.subdir_download_params(),
                ctx.authentication_info(),
                ctx.mirrors,
                ctx.download_options(),
                ctx.remote_fetch_params
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
                return make_unexpected(
                    "Shard index not available",
                    mamba_error_code::repodata_not_loaded
                );
            }

            // Create Shards object
            auto shards = std::make_shared<Shards>(
                *shard_index_opt,
                subdir.metadata().url(),
                subdir.channel(),
                ctx.authentication_info(),
                ctx.mirrors,
                ctx.remote_fetch_params
            );

            // Create RepodataSubset and traverse dependencies
            RepodataSubset subset({ shards });

            // If root_packages is empty, use all packages from the shard index
            std::vector<std::string> packages_to_traverse = root_packages;
            if (packages_to_traverse.empty())
            {
                // Fetch all packages from the shard index
                packages_to_traverse = shards->package_names();
                LOG_DEBUG << "No root packages specified, fetching all "
                          << packages_to_traverse.size() << " packages from shard index";
            }

            auto traversal_result = subset.reachable(packages_to_traverse, "pipelined");
            if (!traversal_result.has_value())
            {
                LOG_WARNING << "Failed to traverse dependencies for " << subdir.name() << ": "
                            << traversal_result.error().what();
                return make_unexpected(
                    "Failed to traverse dependencies",
                    mamba_error_code::repodata_not_loaded
                );
            }

            // Collect all PackageInfo objects from visited shards
            std::vector<specs::PackageInfo> package_infos;
            const std::string base_url = shards->base_url();
            const specs::DynamicPlatform& platform = subdir.platform();
            const std::string channel_id = subdir.channel_id();

            // Get all visited packages from the traversal nodes
            std::set<std::string> visited_packages_set;
            for (const auto& [node_id, node] : subset.nodes())
            {
                if (node.visited)
                {
                    visited_packages_set.insert(node.package);
                }
            }
            std::vector<std::string> visited_packages(
                visited_packages_set.begin(),
                visited_packages_set.end()
            );

            // Load shards for all visited packages and convert to PackageInfo
            for (const auto& package_name : visited_packages)
            {
                // Fetch the shard for this package
                auto shard_result = shards->fetch_shard(package_name);
                if (!shard_result.has_value())
                {
                    LOG_WARNING << "Failed to fetch shard for package " << package_name << ": "
                                << shard_result.error().what();
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

                    package_infos.push_back(std::move(pkg_info));
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

                    package_infos.push_back(std::move(pkg_info));
                }
            }

            if (package_infos.empty())
            {
                return make_unexpected(
                    "No packages found in shards",
                    mamba_error_code::repodata_not_loaded
                );
            }

            // Add packages directly to database
            const auto add_pip = static_cast<solver::libsolv::PipAsPythonDependency>(
                ctx.add_pip_as_python_dependency
            );

            auto repo_info = database.add_repo_from_packages(package_infos, subdir.channel_id(), add_pip);

            // Write solv file if not on Windows
            if (!util::on_win)
            {
                const auto url_parts = util::rsplit(subdir.metadata().url(), "/", 1);
                const auto expected_cache_origin = solver::libsolv::RepodataOrigin{
                    /* .url= */ url_parts.empty() ? subdir.metadata().url() : url_parts.front(),
                    /* .etag= */ subdir.metadata().etag(),
                    /* .mod= */ subdir.metadata().last_modified(),
                };

                auto result = database.native_serialize_repo(
                    repo_info,
                    subdir.writable_libsolv_cache_path(),
                    expected_cache_origin
                );
                if (!result)
                {
                    LOG_WARNING << R"(Fail to write native serialization to file ")"
                                << subdir.writable_libsolv_cache_path() << R"(" for repo ")"
                                << subdir.name() << ": " << std::move(result).error().what();
                }
            }

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
            // true
            std::vector<SubdirIndexLoader*> subdirs_needing_index;
            for (auto& subdir : subdirs)
            {
                bool needs_index = true;
                if (ctx.repodata_use_shards && !root_packages.empty())
                {
                    // Check if shards are available for this subdir
                    if (subdir.metadata().has_up_to_date_shards())
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
            for (std::size_t i = 0; i < subdirs.size(); ++i)
            {
                auto& subdir = subdirs[i];
                if (!subdir.valid_cache_found())
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

                // Try sharded repodata first if available and enabled
                expected_t<solver::libsolv::RepoInfo> result = make_unexpected(
                    "Not attempted",
                    mamba_error_code::repodata_not_loaded
                );

                if (ctx.repodata_use_shards && subdir.metadata().has_up_to_date_shards()
                    && !root_packages.empty())
                {
                    result = load_subdir_with_shards(ctx, database, subdir, root_packages);

                    // If sharded loading fails, fall back to traditional repodata
                    if (!result.has_value())
                    {
                        LOG_WARNING
                            << "Sharded repodata loading failed for " << subdir.name()
                            << ", falling back to traditional repodata: " << result.error().what();
                        result = load_subdir_in_database(ctx, database, subdir);
                    }
                }
                else
                {
                    // Use traditional repodata.json
                    result = load_subdir_in_database(ctx, database, subdir);
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
