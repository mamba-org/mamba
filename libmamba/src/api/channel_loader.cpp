// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/channel_loader.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/download_progress_bar.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/prefix_data.hpp"
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
                if (subdir_index_loader.valid_cache_found())
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

        auto load_channels_impl(
            Context& ctx,
            ChannelContext& channel_context,
            solver::libsolv::Database& database,
            MultiPackageCache& package_caches,
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

            // Download required repodata indexes, with or without progress monitors
            expected_t<void> download_result;
            bool can_monitor = SubdirIndexMonitor::can_monitor(ctx);
            SubdirIndexMonitor check_monitor({ true, true });
            SubdirIndexMonitor index_monitor;
            download_result = SubdirIndexLoader::download_required_indexes(
                subdirs,
                ctx.subdir_download_params(),
                ctx.authentication_info(),
                ctx.mirrors,
                ctx.download_options(),
                ctx.remote_fetch_params,
                can_monitor ? &check_monitor : nullptr,
                can_monitor ? &index_monitor : nullptr
            );

            // Handle download errors: check for user interruption and add to error list
            if (!download_result)
            {
                mamba_error error = download_result.error();
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
            std::string previous_channel;
            bool loading_failed = false;

            // Load each subdir into the database, handling retry logic for corrupted cache
            for (std::size_t i = 0; i < subdirs.size(); ++i)
            {
                SubdirIndexLoader& subdir_index_loader = subdirs[i];

                if (!subdir_index_loader.valid_cache_found())
                {
                    if (!ctx.offline && subdir_index_loader.is_noarch())
                    {
                        error_list.push_back(mamba_error(
                            "Subdir " + subdir_index_loader.name() + " not loaded!",
                            mamba_error_code::subdirdata_not_loaded
                        ));
                    }
                    continue;
                }

                expected_t<solver::libsolv::RepoInfo> subdir_repo_info = load_subdir_in_database(
                    ctx,
                    database,
                    subdir_index_loader
                );
                if (subdir_repo_info)
                {
                    database.set_repo_priority(std::move(subdir_repo_info).value(), priorities[i]);
                }
                else if (is_retry)
                {
                    // Already retried once, report error and exit
                    std::stringstream error_message_stream;
                    error_message_stream << "Could not load repodata.json for "
                                         << subdir_index_loader.name() << " after retry."
                                         << "Please check repodata source. Exiting." << std::endl;
                    error_list.push_back(
                        mamba_error(error_message_stream.str(), mamba_error_code::repodata_not_loaded)
                    );
                }
                else
                {
                    // First failure: clear cache and mark for retry
                    LOG_WARNING << "Could not load repodata.json for " << subdir_index_loader.name()
                                << ". Deleting cache, and retrying.";
                    subdir_index_loader.clear_valid_cache_files();
                    loading_failed = true;
                }
            }

            // Retry logic: if loading failed and we haven't retried yet, retry once
            if (loading_failed)
            {
                bool should_retry = !ctx.offline && !is_retry;
                if (should_retry)
                {
                    LOG_WARNING << "Encountered malformed repodata.json cache. Redownloading.";
                    bool retry = true;
                    return load_channels_impl(ctx, channel_context, database, package_caches, retry);
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
        MultiPackageCache& package_caches
    ) -> expected_t<void, mamba_aggregated_error>
    {
        bool retry = false;
        return load_channels_impl(ctx, channel_context, database, package_caches, retry);
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
