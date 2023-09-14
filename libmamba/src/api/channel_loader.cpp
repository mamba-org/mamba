// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/channel_loader.hpp"
#include "mamba/core/channel.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/subdirdata.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    namespace
    {
        MRepo create_repo_from_pkgs_dir(MPool& pool, const fs::u8path& pkgs_dir)
        {
            if (!fs::exists(pkgs_dir))
            {
                // TODO : us tl::expected mechanis
                throw std::runtime_error("Specified pkgs_dir does not exist\n");
            }
            auto sprefix_data = PrefixData::create(pkgs_dir, pool.channel_context());
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
            return MRepo(pool, prefix_data);
        }
    }

    expected_t<void, mamba_aggregated_error>
    load_channels(MPool& pool, MultiPackageCache& package_caches, int is_retry)
    {
        int RETRY_SUBDIR_FETCH = 1 << 0;

        auto& ctx = pool.context();

        std::vector<std::string> channel_urls = ctx.channels;

        std::vector<MSubdirData> subdirs;
        MultiDownloadTarget multi_dl{ ctx };

        std::vector<std::pair<int, int>> priorities;
        int max_prio = static_cast<int>(channel_urls.size());
        std::string prev_channel_name;

        Console::instance().init_progress_bar_manager(ProgressBarMode::multi);

        std::vector<mamba_error> error_list;

        for (auto channel : pool.channel_context().get_channels(channel_urls))
        {
            for (auto& [platform, url] : channel->platform_urls(true))
            {
                auto sdires = MSubdirData::create(
                    pool.channel_context(),
                    *channel,
                    platform,
                    url,
                    package_caches,
                    "repodata.json"
                );
                if (!sdires.has_value())
                {
                    error_list.push_back(std::move(sdires).error());
                    continue;
                }
                auto sdir = std::move(sdires).value();
                subdirs.push_back(std::move(sdir));
                if (ctx.channel_priority == ChannelPriority::Disabled)
                {
                    priorities.push_back(std::make_pair(0, 0));
                }
                else
                {
                    // Consider 'flexible' and 'strict' the same way
                    if (channel->name() != prev_channel_name)
                    {
                        max_prio--;
                        prev_channel_name = channel->name();
                    }
                    priorities.push_back(std::make_pair(max_prio, 0));
                }
            }
        }

        for (auto& subdir : subdirs)
        {
            for (auto& check_target : subdir.check_targets())
            {
                multi_dl.add(check_target.get());
            }
        }

        multi_dl.download(MAMBA_NO_CLEAR_PROGRESS_BARS);
        if (is_sig_interrupted())
        {
            error_list.push_back(mamba_error("Interrupted by user", mamba_error_code::user_interrupted)
            );
            return tl::unexpected(mamba_aggregated_error(std::move(error_list)));
        }

        for (auto& subdir : subdirs)
        {
            if (!subdir.check_targets().empty())
            {
                // recreate final download target in case HEAD requests succeeded
                subdir.finalize_checks();
            }
            multi_dl.add(subdir.target());
        }

        // TODO load local channels even when offline if (!ctx.offline)
        if (!ctx.offline)
        {
            try
            {
                multi_dl.download(MAMBA_DOWNLOAD_FAILFAST);
            }
            catch (const std::runtime_error& e)
            {
                error_list.push_back(mamba_error(e.what(), mamba_error_code::repodata_not_loaded));
            }
        }

        if (ctx.offline)
        {
            LOG_INFO << "Creating repo from pkgs_dir for offline";
            for (const auto& c : ctx.pkgs_dirs)
            {
                create_repo_from_pkgs_dir(pool, c);
            }
        }
        std::string prev_channel;
        bool loading_failed = false;
        for (std::size_t i = 0; i < subdirs.size(); ++i)
        {
            auto& subdir = subdirs[i];
            if (!subdir.loaded())
            {
                if (!ctx.offline && util::ends_with(subdir.name(), "/noarch"))
                {
                    error_list.push_back(mamba_error(
                        "Subdir " + subdir.name() + " not loaded!",
                        mamba_error_code::subdirdata_not_loaded
                    ));
                }
                continue;
            }

            auto repo = subdir.create_repo(pool);
            if (repo)
            {
                auto& prio = priorities[i];
                repo.value().set_priority(prio.first, prio.second);
            }
            else
            {
                if (is_retry & RETRY_SUBDIR_FETCH)
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
                    subdir.clear_cache();
                    loading_failed = true;
                }
            }
        }

        if (loading_failed)
        {
            if (!ctx.offline && !(is_retry & RETRY_SUBDIR_FETCH))
            {
                LOG_WARNING << "Encountered malformed repodata.json cache. Redownloading.";
                return load_channels(pool, package_caches, is_retry | RETRY_SUBDIR_FETCH);
            }
            error_list.push_back(mamba_error(
                "Could not load repodata. Cache corrupted?",
                mamba_error_code::repodata_not_loaded
            ));
        }
        using return_type = expected_t<void, mamba_aggregated_error>;
        return error_list.empty() ? return_type()
                                  : return_type(make_unexpected(std::move(error_list)));
    }

}
