// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <optional>
#include <set>
#include <sstream>

#include "mamba/api/channel_loader.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/download_progress_bar.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/shard_index_loader.hpp"
#include "mamba/core/shard_traversal.hpp"
#include "mamba/core/shard_types.hpp"
#include "mamba/core/subdir_index.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/specs/package_info.hpp"

namespace mamba
{
    namespace
    {
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
            // Create PrefixData from pkgs_dir to load installed packages
            expected_t<PrefixData> prefix_data_result = PrefixData::create(pkgs_dir, channel_context);
            if (!prefix_data_result)
            {
                throw std::runtime_error("Specified pkgs_dir does not exist\n");
            }
            PrefixData& prefix_data = prefix_data_result.value();
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
            std::vector<SubdirIndexLoader>& subdir_index_loaders,
            std::vector<mamba_error>& error_list,
            std::vector<solver::libsolv::Priorities>& priorities,
            int& max_prio,
            specs::CondaURL& previous_channel_url
        )
        {
            static bool has_shown_anaconda_channel_warning = false;
            for (const auto& platform : channel.platforms())
            {
                bool show_warning = ctx.show_anaconda_channel_warnings;
                std::string channel_name = channel.platform_url(platform).host();
                if (channel_name == "repo.anaconda.com" && show_warning
                    && !has_shown_anaconda_channel_warning)
                {
                    has_shown_anaconda_channel_warning = true;
                    LOG_WARNING << "'" << channel_name
                                << "', a commercial channel hosted by Anaconda.com, is used.\n";
                    LOG_WARNING << "Please make sure you understand Anaconda Terms of Services.\n";
                    LOG_WARNING << "See: https://legal.anaconda.com/policies/en/";
                }

                SubdirParams subdir_params = ctx.subdir_params();
                subdir_params.repodata_force_use_zst = channel_context.has_zst(channel);


                // Create SubdirIndexLoader for this platform; handle errors gracefully
                expected_t<SubdirIndexLoader> subdir_index_loader_result = SubdirIndexLoader::create(
                    subdir_params,
                    channel,
                    platform,
                    package_caches,
                    "repodata.json"
                );
                if (!subdir_index_loader_result.has_value())
                {
                    error_list.push_back(std::move(subdir_index_loader_result).error());
                    continue;
                }
                SubdirIndexLoader subdir_index_loader = std::move(subdir_index_loader_result).value();
                if (subdir_index_loader.valid_cache_found() && Context::can_report_status())
                {
                    Console::stream()
                        << fmt::format("{:<50} {:>20}", subdir_index_loader.name(), "Using cache");
                }

                subdir_index_loaders.push_back(std::move(subdir_index_loader));
                if (ctx.channel_priority == ChannelPriority::Disabled)
                {
                    priorities.push_back({ /* .priority= */ 0, /* .subpriority= */ 0 });
                }
                else
                {
                    // Consider 'flexible' and 'strict' the same way
                    if (channel.url() != previous_channel_url)
                    {
                        max_prio--;
                        previous_channel_url = channel.url();
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

        auto load_subdir_with_shards(
            Context& ctx,
            solver::libsolv::Database& database,
            const std::vector<std::string>& root_packages,
            std::vector<SubdirIndexLoader>& subdirs,
            std::size_t subdir_idx,
            std::set<std::string>& loaded_subdirs_with_shards,
            const std::vector<solver::libsolv::Priorities>& priorities
        ) -> expected_t<solver::libsolv::RepoInfo>
        {
            auto& subdir = subdirs[subdir_idx];
            if (!ctx.repodata_use_shards
                || !subdir.metadata().has_up_to_date_shards(ctx.repodata_shards_ttl)
                || root_packages.empty())
            {
                return tl::unexpected(mamba_error("Shards not applicable", mamba_error_code::unknown));
            }

            auto subdir_params = ctx.subdir_download_params();
            auto shard_index_result = ShardIndexLoader::fetch_and_parse_shard_index(
                subdir,
                subdir_params,
                ctx.authentication_info(),
                ctx.mirrors,
                ctx.download_options(),
                ctx.remote_fetch_params,
                ctx.repodata_shards_ttl
            );
            if (!shard_index_result || !shard_index_result.value().has_value())
            {
                return tl::unexpected(mamba_error(
                    "Failed to fetch shard index for " + subdir.name(),
                    mamba_error_code::subdirdata_not_loaded
                ));
            }

            const auto& channel = subdir.channel();
            std::string current_repodata_url = subdir.repodata_url().str();

            std::vector<std::shared_ptr<Shards>> all_shards;
            std::map<std::string, std::size_t> url_to_subdir_idx;
            for (std::size_t j = 0; j < subdirs.size(); ++j)
            {
                if (subdirs[j].channel().url() == channel.url())
                {
                    auto sidx_result = (j == subdir_idx)
                                           ? std::move(shard_index_result)
                                           : ShardIndexLoader::fetch_and_parse_shard_index(
                                                 subdirs[j],
                                                 subdir_params,
                                                 ctx.authentication_info(),
                                                 ctx.mirrors,
                                                 ctx.download_options(),
                                                 ctx.remote_fetch_params,
                                                 ctx.repodata_shards_ttl
                                             );
                    if (!sidx_result || !sidx_result.value().has_value())
                    {
                        continue;
                    }
                    auto si = std::move(sidx_result.value().value());
                    std::string sdir_url = subdirs[j].repodata_url().str();
                    auto shards_ptr = std::make_shared<Shards>(
                        std::move(si),
                        sdir_url,
                        subdirs[j].channel(),
                        ctx.authentication_info(),
                        ctx.remote_fetch_params,
                        ctx.repodata_shards_threads,
                        &ctx.mirrors
                    );
                    all_shards.push_back(std::move(shards_ptr));
                    url_to_subdir_idx[sdir_url] = j;
                }
            }

            RepodataSubset subset(std::move(all_shards));
            subset.reachable(root_packages, "bfs", std::nullopt);

            std::map<std::string, std::vector<specs::PackageInfo>> packages_by_url;
            for (const auto& [node_id, node] : subset.nodes())
            {
                if (!node.visited)
                {
                    continue;
                }
                auto it = std::find_if(
                    subset.shards().begin(),
                    subset.shards().end(),
                    [&node_id](const std::shared_ptr<Shards>& s)
                    { return s->url() == node_id.channel; }
                );
                if (it == subset.shards().end())
                {
                    continue;
                }
                auto& shards_ptr = *it;
                try
                {
                    auto shard = shards_ptr->visit_package(node_id.package);
                    auto base_url = shards_ptr->base_url();
                    specs::DynamicPlatform platform = shards_ptr->subdir();
                    std::string channel_id = subdirs[url_to_subdir_idx[node_id.channel]].channel_id();
                    for (const auto& [filename, record] : shard.packages)
                    {
                        packages_by_url[node_id.channel].push_back(
                            to_package_info(record, filename, channel_id, platform, base_url)
                        );
                    }
                    for (const auto& [filename, record] : shard.conda_packages)
                    {
                        packages_by_url[node_id.channel].push_back(
                            to_package_info(record, filename, channel_id, platform, base_url)
                        );
                    }
                }
                catch (const std::exception&)
                {
                }
            }

            std::optional<solver::libsolv::RepoInfo> result_repo;
            for (const auto& [channel_url, pkgs] : packages_by_url)
            {
                std::string repo_name = subdirs[url_to_subdir_idx[channel_url]].name();
                if (loaded_subdirs_with_shards.count(repo_name))
                {
                    continue;
                }
                loaded_subdirs_with_shards.insert(repo_name);

                auto sorted_pkgs = pkgs;
                std::sort(
                    sorted_pkgs.begin(),
                    sorted_pkgs.end(),
                    [](const specs::PackageInfo& lhs, const specs::PackageInfo& rhs)
                    {
                        // Compare in reverse order for descending sort (newer versions first)
                        return specs::compare_packages_by_version_and_build(rhs, lhs);
                    }
                );
                auto repo = database.add_repo_from_packages(
                    sorted_pkgs,
                    repo_name,
                    solver::libsolv::PipAsPythonDependency(ctx.add_pip_as_python_dependency)
                );
                std::size_t idx = url_to_subdir_idx[channel_url];
                database.set_repo_priority(repo, priorities[idx]);
                if (channel_url == current_repodata_url)
                {
                    result_repo = std::move(repo);
                }
            }
            if (result_repo)
            {
                return std::move(*result_repo);
            }
            return tl::unexpected(
                mamba_error("No packages for " + subdir.name(), mamba_error_code::subdirdata_not_loaded)
            );
        }

        auto load_channels_impl(
            Context& ctx,
            ChannelContext& channel_context,
            solver::libsolv::Database& database,
            MultiPackageCache& package_caches,
            const std::vector<std::string>& root_packages,
            bool is_retry
        ) -> expected_t<void, mamba_aggregated_error>
        {
            std::vector<SubdirIndexLoader> subdirs;

            std::vector<solver::libsolv::Priorities> priorities;
            int max_prio = static_cast<int>(ctx.channels.size());
            specs::CondaURL previous_channel_url;

            Console::instance().init_progress_bar_manager(ProgressBarMode::multi);

            std::vector<mamba_error> error_list;

            // Process mirrored channels: create channel objects, configure mirrors, and initialize
            // subdirs for each platform. Mirrored channels use alternative URLs for redundancy and
            // performance.
            for (const auto& mirror : ctx.mirrored_channels)
            {
                for (const specs::Channel& channel :
                     channel_context.make_channel(mirror.first, mirror.second))
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
                        previous_channel_url
                    );
                }
            }

            std::vector<specs::PackageInfo> packages;

            // Process regular (non-mirrored) channels: skip channels already handled as mirrored
            // channels, then handle two cases:
            //  - (1) package URLs are extracted as PackageInfo and collected separately,
            //  - (2) regular channel URLs have mirrors configured and subdirs initialized for each
            //  platform.
            for (const auto& location : ctx.channels)
            {
                if (!ctx.mirrored_channels.contains(location))
                {
                    for (const specs::Channel& channel : channel_context.make_channel(location))
                    {
                        if (channel.is_package())
                        {
                            specs::PackageInfo pkg_info = specs::PackageInfo::from_url(
                                                              channel.url().str()
                            )
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
                            previous_channel_url
                        );
                    }
                }
            }

            if (!packages.empty())
            {
                database.add_repo_from_packages(packages, "packages");
            }

            // Download required repodata indexes, with or without progress monitors.
            auto subdir_params = ctx.subdir_download_params();
            download::Monitor* check_monitor_ptr = nullptr;
            SubdirIndexMonitor check_monitor({ true, true });
            if (SubdirIndexMonitor::can_monitor(ctx))
            {
                check_monitor_ptr = &check_monitor;
            }

            auto check_res = SubdirIndexLoader::download_requests(
                SubdirIndexLoader::build_all_check_requests(subdirs.begin(), subdirs.end(), subdir_params),
                ctx.authentication_info(),
                ctx.mirrors,
                ctx.download_options(),
                ctx.remote_fetch_params,
                check_monitor_ptr
            );

            // Handle check download errors: check for user interruption and add to error list
            if (!check_res)
            {
                mamba_error error = check_res.error();
                mamba_error_code error_code = error.error_code();
                error_list.push_back(std::move(error));
                if (error_code == mamba_error_code::user_interrupted)
                {
                    return tl::unexpected(mamba_aggregated_error(std::move(error_list), false));
                }
            }

            // For each subdir, decide whether we can use shards or need full index.
            std::vector<SubdirIndexLoader*> subdirs_needing_index;
            for (auto& s : subdirs)
            {
                bool use_shards = ctx.repodata_use_shards
                                  && s.metadata().has_up_to_date_shards(ctx.repodata_shards_ttl)
                                  && !root_packages.empty();
                if (!use_shards)
                {
                    subdirs_needing_index.push_back(&s);
                }
            }

            expected_t<void> download_res = expected_t<void>();
            if (!subdirs_needing_index.empty())
            {
                if (SubdirIndexMonitor::can_monitor(ctx))
                {
                    SubdirIndexMonitor index_monitor;
                    download_res = SubdirIndexLoader::download_requests(
                        SubdirIndexLoader::build_all_index_requests(
                            subdirs_needing_index.begin(),
                            subdirs_needing_index.end(),
                            subdir_params
                        ),
                        ctx.authentication_info(),
                        ctx.mirrors,
                        ctx.download_options(),
                        ctx.remote_fetch_params,
                        &index_monitor
                    );
                }
                else
                {
                    download_res = SubdirIndexLoader::download_requests(
                        SubdirIndexLoader::build_all_index_requests(
                            subdirs_needing_index.begin(),
                            subdirs_needing_index.end(),
                            subdir_params
                        ),
                        ctx.authentication_info(),
                        ctx.mirrors,
                        ctx.download_options(),
                        ctx.remote_fetch_params,
                        nullptr
                    );
                }
            }

            // Handle index download errors: check for user interruption and add to error list
            if (!download_res)
            {
                mamba_error error = download_res.error();
                mamba_error_code error_code = error.error_code();
                error_list.push_back(std::move(error));
                if (error_code == mamba_error_code::user_interrupted)
                {
                    return tl::unexpected(mamba_aggregated_error(std::move(error_list), false));
                }
            }

            // In offline mode, load packages from local pkgs_dir
            if (ctx.offline)
            {
                LOG_INFO << "Creating repo from pkgs_dir for offline";
                for (const auto& c : ctx.pkgs_dirs)
                {
                    create_repo_from_pkgs_dir(ctx, channel_context, database, c);
                }
            }

            std::set<std::string> loaded_subdirs_with_shards;
            bool loading_failed = false;

            // Load each subdir into the database, handling retry logic for corrupted cache
            for (std::size_t i = 0; i < subdirs.size(); ++i)
            {
                auto& subdir = subdirs[i];
                bool use_shards = ctx.repodata_use_shards
                                  && subdir.metadata().has_up_to_date_shards(ctx.repodata_shards_ttl)
                                  && !root_packages.empty();

                if (loaded_subdirs_with_shards.count(subdir.name()))
                {
                    continue;
                }

                if (!subdir.valid_cache_found() && !use_shards)
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

                auto result = [&]() -> expected_t<solver::libsolv::RepoInfo>
                {
                    if (use_shards)
                    {
                        auto res = load_subdir_with_shards(
                            ctx,
                            database,
                            root_packages,
                            subdirs,
                            i,
                            loaded_subdirs_with_shards,
                            priorities
                        );
                        if (!res)
                        {
                            if (subdir.valid_cache_found())
                            {
                                return load_subdir_in_database(ctx, database, subdir);
                            }
                            // Shards failed and no cache - try to fetch and load flat repodata
                            if (!ctx.offline)
                            {
                                LOG_WARNING << "Shard loading failed for " << subdir.name()
                                            << ". Falling back to full repodata.json download.";
                                std::vector<SubdirIndexLoader*> fallback_subdirs = { &subdir };
                                auto fetch_res = SubdirIndexLoader::download_requests(
                                    SubdirIndexLoader::build_all_index_requests(
                                        fallback_subdirs.begin(),
                                        fallback_subdirs.end(),
                                        subdir_params
                                    ),
                                    ctx.authentication_info(),
                                    ctx.mirrors,
                                    ctx.download_options(),
                                    ctx.remote_fetch_params,
                                    nullptr
                                );
                                if (fetch_res)
                                {
                                    return load_subdir_in_database(ctx, database, subdir);
                                }
                            }
                        }
                        return res;
                    }
                    return load_subdir_in_database(ctx, database, subdir);
                }();

                if (result)
                {
                    database.set_repo_priority(std::move(result).value(), priorities[i]);
                }
                else if (is_retry)
                {
                    // Already retried once, report error and exit
                    std::stringstream error_message_stream;
                    error_message_stream << "Could not load repodata.json for " << subdir.name()
                                         << " after retry."
                                         << "Please check repodata source. Exiting." << std::endl;
                    error_list.push_back(
                        mamba_error(error_message_stream.str(), mamba_error_code::repodata_not_loaded)
                    );
                }
                else
                {
                    // First failure: clear cache and mark for retry
                    LOG_WARNING << "Could not load repodata.json for " << subdir.name()
                                << ". Deleting cache, and retrying.";
                    subdir.clear_valid_cache_files();
                    loading_failed = true;
                }
            }

            if (loading_failed)
            {
                bool should_retry = !ctx.offline && !is_retry;
                if (should_retry)
                {
                    LOG_WARNING << "Encountered malformed repodata.json cache. Redownloading.";
                    bool retry = true;
                    return load_channels_impl(
                        ctx,
                        channel_context,
                        database,
                        package_caches,
                        root_packages,
                        retry
                    );
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
        return load_channels_impl(ctx, channel_context, database, package_caches, root_packages, retry);
    }

    void init_channels(Context& context, ChannelContext& channel_context)
    {
        for (const auto& mirror : context.mirrored_channels)
        {
            for (const specs::Channel& channel :
                 channel_context.make_channel(mirror.first, mirror.second))
            {
                create_mirrors(channel, context.mirrors);
            }
        }

        for (const auto& location : context.channels)
        {
            if (!context.mirrored_channels.contains(location))
            {
                for (const specs::Channel& channel : channel_context.make_channel(location))
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
            specs::PackageInfo pkg_info = specs::PackageInfo::from_url(spec)
                                              .or_else([](specs::ParseError&& err)
                                                       { throw std::move(err); })
                                              .value();
            for (const specs::Channel& channel : channel_context.make_channel(pkg_info.channel))
            {
                create_mirrors(channel, context.mirrors);
            }
        }
    }

}
