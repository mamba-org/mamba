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
#include "mamba/core/virtual_packages.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/solver/solver_factory.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    namespace
    {
        auto create_repo_from_pkgs_dir(
            const Context& ctx,
            ChannelContext& channel_context,
            solver::DatabaseVariant& database,
            const fs::u8path& pkgs_dir
        ) -> void
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

            // Create a repo from the packages
            if (auto* libsolv_db = std::get_if<solver::libsolv::Database>(&database))
            {
                libsolv_db->add_repo_from_packages(
                    prefix_data.sorted_records(),
                    "pkgs_dir",
                    solver::libsolv::PipAsPythonDependency::No
                );

                // Load the packages into the database
                load_installed_packages_in_database(
                    ctx,
                    std::variant<
                        std::reference_wrapper<solver::libsolv::Database>,
                        std::reference_wrapper<solver::resolvo::Database>>(std::ref(*libsolv_db)),
                    prefix_data
                );
            }
            else if (auto* resolvo_db = std::get_if<solver::resolvo::Database>(&database))
            {
                resolvo_db->add_repo_from_packages(prefix_data.sorted_records(), "pkgs_dir", false);

                // Load the packages into the database
                load_installed_packages_in_database(
                    ctx,
                    std::variant<
                        std::reference_wrapper<solver::libsolv::Database>,
                        std::reference_wrapper<solver::resolvo::Database>>(std::ref(*resolvo_db)),
                    prefix_data
                );
            }
            else
            {
                throw std::runtime_error("Invalid database variant");
            }
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
            solver::DatabaseVariant& database,
            MultiPackageCache& package_caches,
            bool is_retry
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
                if (auto* libsolv_db = std::get_if<solver::libsolv::Database>(&database))
                {
                    libsolv_db->add_repo_from_packages(packages, "packages");
                }
                else if (auto* resolvo_db = std::get_if<solver::resolvo::Database>(&database))
                {
                    resolvo_db->add_repo_from_packages(packages, "packages");
                }
            }

            expected_t<void> download_res;
            if (SubdirIndexMonitor::can_monitor(ctx))
            {
                SubdirIndexMonitor check_monitor({ true, true });
                SubdirIndexMonitor index_monitor;
                download_res = SubdirIndexLoader::download_required_indexes(
                    subdirs,
                    ctx.subdir_download_params(),
                    ctx.authentication_info(),
                    ctx.mirrors,
                    ctx.download_options(),
                    ctx.remote_fetch_params,
                    &check_monitor,
                    &index_monitor
                );
            }
            else
            {
                download_res = SubdirIndexLoader::download_required_indexes(
                    subdirs,
                    ctx.subdir_download_params(),
                    ctx.authentication_info(),
                    ctx.mirrors,
                    ctx.download_options(),
                    ctx.remote_fetch_params
                );
            }

            if (!download_res)
            {
                mamba_error error = download_res.error();
                mamba_error_code ec = error.error_code();
                error_list.push_back(std::move(error));
                if (ec == mamba_error_code::user_interrupted)
                {
                    return tl::unexpected(mamba_aggregated_error(std::move(error_list)));
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

                if (auto* libsolv_db = std::get_if<solver::libsolv::Database>(&database))
                {
                    auto res = load_subdir_in_database(ctx, *libsolv_db, subdir);
                    if (!res)
                    {
                        if (is_retry)
                        {
                            std::stringstream ss;
                            ss << "Could not load repodata.json for " << subdir.name()
                               << " after retry."
                               << "Please check repodata source. Exiting." << std::endl;
                            error_list.push_back(
                                mamba_error(ss.str(), mamba_error_code::repodata_not_loaded)
                            );
                        }
                        else
                        {
                            LOG_WARNING << "Could not load repodata.json for " << subdir.name()
                                        << ". Deleting cache, and retrying.";
                            subdir.clear_valid_cache_files();
                            loading_failed = true;
                        }
                    }
                }
                else if (auto* resolvo_db = std::get_if<solver::resolvo::Database>(&database))
                {
                    // For resolvo, we need to load the repodata.json file and add it to the
                    // database
                    auto repodata_json = subdir.valid_json_cache_path();
                    if (!repodata_json)
                    {
                        if (is_retry)
                        {
                            std::stringstream ss;
                            ss << "Could not load repodata.json for " << subdir.name()
                               << " after retry."
                               << "Please check repodata source. Exiting." << std::endl;
                            error_list.push_back(
                                mamba_error(ss.str(), mamba_error_code::repodata_not_loaded)
                            );
                        }
                        else
                        {
                            LOG_WARNING << "Could not load repodata.json for " << subdir.name()
                                        << ". Deleting cache, and retrying.";
                            subdir.clear_valid_cache_files();
                            loading_failed = true;
                        }
                        continue;
                    }

                    try
                    {
                        resolvo_db->add_repo_from_repodata_json(
                            repodata_json.value(),
                            util::rsplit(subdir.metadata().url(), "/", 1).front(),
                            subdir.channel_id()
                        );
                    }
                    catch (const std::exception& e)
                    {
                        if (is_retry)
                        {
                            std::stringstream ss;
                            ss << "Could not load repodata.json for " << subdir.name()
                               << " after retry: " << e.what()
                               << ". Please check repodata source. Exiting." << std::endl;
                            error_list.push_back(
                                mamba_error(ss.str(), mamba_error_code::repodata_not_loaded)
                            );
                        }
                        else
                        {
                            LOG_WARNING << "Could not load repodata.json for " << subdir.name()
                                        << ": " << e.what() << ". Deleting cache, and retrying.";
                            subdir.clear_valid_cache_files();
                            loading_failed = true;
                        }
                    }
                }
            }

            if (loading_failed)
            {
                if (!ctx.offline && !is_retry)
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
        solver::DatabaseVariant& database,
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
