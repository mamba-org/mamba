#include "mamba/api/channel_loader.hpp"

#include "mamba/core/channel.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/subdirdata.hpp"


namespace mamba
{
    namespace detail
    {
        MRepo& create_repo_from_pkgs_dir(MPool& pool, const fs::path& pkgs_dir)
        {
            if (!fs::exists(pkgs_dir))
            {
                throw std::runtime_error("Specified pkgs_dir does not exist\n");
            }
            PrefixData prefix_data(pkgs_dir);
            for (const auto& entry : fs::directory_iterator(pkgs_dir))
            {
                fs::path repodata_record_json = entry.path() / "info" / "repodata_record.json";
                if (!fs::exists(repodata_record_json))
                {
                    continue;
                }
                prefix_data.load_single_record(repodata_record_json);
            }
            return MRepo::create(pool, prefix_data);
        }
    }

    void load_channels(MPool& pool, MultiPackageCache& package_caches, int is_retry)
    {
        int RETRY_SUBDIR_FETCH = 1 << 0;

        auto& ctx = Context::instance();

        std::vector<std::string> channel_urls = ctx.channels;

        std::vector<std::shared_ptr<MSubdirData>> subdirs;
        MultiDownloadTarget multi_dl;

        std::vector<std::pair<int, int>> priorities;
        int max_prio = static_cast<int>(channel_urls.size());
        std::string prev_channel_name;

        Console::instance().init_progress_bar_manager(ProgressBarMode::multi);

        load_tokens();

        for (auto channel : get_channels(channel_urls))
        {
            for (auto& [platform, url] : channel->platform_urls(true))
            {
                auto sdir = std::make_shared<MSubdirData>(*channel, platform, url, package_caches);

                multi_dl.add(sdir->target());
                subdirs.push_back(sdir);
                if (ctx.channel_priority == ChannelPriority::kDisabled)
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
        // TODO load local channels even when offline
        if (!ctx.offline)
        {
            multi_dl.download(MAMBA_DOWNLOAD_FAILFAST);
        }

        if (ctx.offline)
        {
            LOG_INFO << "Creating repo from pkgs_dir for offline";
            for (const auto& c : ctx.pkgs_dirs)
                detail::create_repo_from_pkgs_dir(pool, c);
        }
        std::string prev_channel;
        bool loading_failed = false;
        for (std::size_t i = 0; i < subdirs.size(); ++i)
        {
            auto& subdir = subdirs[i];
            if (!subdir->loaded())
            {
                if (ctx.offline || !mamba::ends_with(subdir->name(), "/noarch"))
                {
                    continue;
                }
                else
                {
                    throw std::runtime_error("Subdir " + subdir->name() + " not loaded!");
                }
            }

            auto& prio = priorities[i];
            try
            {
                MRepo& repo = subdir->create_repo(pool);
                repo.set_priority(prio.first, prio.second);
            }
            catch (std::runtime_error& e)
            {
                if (is_retry & RETRY_SUBDIR_FETCH)
                {
                    std::stringstream ss;
                    ss << "Could not load repodata.json for " << subdir->name() << " after retry."
                       << "Please check repodata source. Exiting." << std::endl;
                    throw std::runtime_error(ss.str());
                }

                LOG_WARNING << "Could not load repodata.json for " << subdir->name()
                            << ". Deleting cache, and retrying.";
                subdir->clear_cache();
                loading_failed = true;
            }
        }

        if (loading_failed)
        {
            if (!ctx.offline && !(is_retry & RETRY_SUBDIR_FETCH))
            {
                LOG_WARNING << "Encountered malformed repodata.json cache. Redownloading.";
                return load_channels(pool, package_caches, is_retry | RETRY_SUBDIR_FETCH);
            }
            throw std::runtime_error("Could not load repodata. Cache corrupted?");
        }
    }

}
