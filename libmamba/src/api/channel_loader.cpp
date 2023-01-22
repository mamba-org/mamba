#include "mamba/api/channel_loader.hpp"

#include "mamba/core/channel.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/subdirdata.hpp"
#include "mamba/core/thread_utils.hpp"

#include <powerloader/downloader.hpp>
#include <powerloader/mirrors/oci.hpp>

namespace mamba
{
    namespace detail
    {
        MRepo& create_repo_from_pkgs_dir(MPool& pool, const fs::u8path& pkgs_dir)
        {
            if (!fs::exists(pkgs_dir))
            {
                // TODO : us tl::expected mechanis
                throw std::runtime_error("Specified pkgs_dir does not exist\n");
            }
            auto sprefix_data = PrefixData::create(pkgs_dir);
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
            return MRepo::create(pool, prefix_data);
        }
    }


    namespace oci_detail
    {
        std::pair<std::string, std::string> oci_fn_split_tag(const std::string& fn)
        {
            if (ends_with(fn, ".json"))
                return { fn, "latest" };
            // for OCI, if we have a filename like "xtensor-0.23.10-h2acdbc0_0.tar.bz2"
            // we want to split it to `xtensor:0.23.10-h2acdbc0-0`
            std::pair<std::string, std::string> result;

            auto parts = rsplit(fn, "-", 2);

            if (parts.size() < 2)
            {
                spdlog::error("Could not split filename into enough parts");
            }

            result.first = parts[0];

            // if we have fn that looks like `conda-forge/osx-arm64/_r-mutex` we need to add a
            // `zzz_` because on OCI registries, image names cannot start with underscore.
            replace_all(result.first, "/_", "/zzz_");

            std::string tag;
            if (parts.size() > 2)
            {
                std::string last_part = parts[2].substr(0, parts[2].find_first_of("."));
                tag = fmt::format("{}-{}", parts[1], last_part);
            }
            else
            {
                tag = parts[1];
            }

            // we need to replace some special characters in tags as they are not allowed on OCI
            // registries
            replace_all(tag, "!", "__e__");
            replace_all(tag, "+", "__p__");
            replace_all(tag, "=", "__eq__");

            result.second = tag;

            return result;
        }
    }

    expected_t<void, mamba_aggregated_error> load_channels(MPool& pool,
                                                           MultiPackageCache& package_caches,
                                                           int is_retry)
    {
        int RETRY_SUBDIR_FETCH = 1 << 0;

        auto& ctx = Context::instance();

        std::vector<std::string> channel_urls = ctx.channels;

        std::vector<MSubdirData> subdirs;
        ctx.plcontext.mirror_map.clear();
        if (ctx.plcontext.mirror_map.empty())
        {
            if (ctx.mirrors.size())
            {
                for (auto& [mname, mirrors] : ctx.mirrors)
                {
                    for (auto& m : mirrors)
                    {
                        if (starts_with(m, "http"))
                        {
                            ctx.plcontext.mirror_map.create_unique_mirror<powerloader::HTTPMirror>(
                                mname, ctx.plcontext, m);
                        }
                        else if (starts_with(m, "oci://"))
                        {
                            std::string username = env::get("GHA_USER").value_or("");
                            std::string password = env::get("GHA_PAT").value_or("");
                            auto plm = ctx.plcontext.mirror_map
                                           .create_unique_mirror<powerloader::OCIMirror>(
                                               mname,
                                               ctx.plcontext,
                                               "https://ghcr.io",
                                               "channel-mirrors",
                                               "pull",
                                               username,
                                               password);
                            plm->set_fn_tag_split_function(oci_detail::oci_fn_split_tag);
                        }
                    }
                }
            }
            else
            {
                // TODO find a robust way to do this!!
                for (auto channel : get_channels(channel_urls))
                {
                    LOG_INFO << "Adding mirror " << channel->canonical_name() << " for "
                             << channel->base_url();
                    std::string base_url = channel->base_url();
                    std::string name = channel->canonical_name();
                    if (ends_with(base_url, fmt::format("/{}", name)))
                    {
                        base_url = base_url.substr(0, base_url.size() - name.size() - 1);
                    }
                    auto mirror
                        = ctx.plcontext.mirror_map.create_unique_mirror<powerloader::HTTPMirror>(
                            name, ctx.plcontext, base_url);
                    if (channel->auth().has_value())
                    {
                        auto& val = channel->auth().value();
                        auto x = split(val, ":", 1);
                        assert(x.size() == 2);
                        dynamic_cast<powerloader::HTTPMirror*>(mirror.get())->set_auth(x[0], x[1]);
                    }
                }
            }
        }
        ctx.plcontext.set_verbosity(ctx.verbosity);
        powerloader::Downloader multi_dl(ctx.plcontext);

        std::vector<std::pair<int, int>> priorities;
        int max_prio = static_cast<int>(channel_urls.size());
        std::string prev_channel_name;

        Console::instance().init_progress_bar_manager(ProgressBarMode::multi);

        std::vector<mamba_error> error_list;

        for (auto channel : get_channels(channel_urls))
        {
            for (auto& [platform, url] : channel->platform_urls(true))
            {
                auto sdires = MSubdirData::create(*channel, platform, url, package_caches, "repodata.json");
                if (!sdires.has_value())
                {
                    error_list.push_back(std::move(sdires).error());
                    continue;
                }
                auto sdir = std::move(sdires).value();
                subdirs.push_back(std::move(sdir));
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

        for (auto& subdir : subdirs)
        {
            for (auto& check_target : subdir.check_targets())
            {
                multi_dl.add(check_target);
            }
        }

        // multi_dl.download(MAMBA_NO_CLEAR_PROGRESS_BARS);
        multi_dl.download();
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
                // multi_dl.download(MAMBA_DOWNLOAD_FAILFAST);
                // multi_dl.download();
                download_with_progressbars(multi_dl);
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
                detail::create_repo_from_pkgs_dir(pool, c);
            }
        }
        std::string prev_channel;
        bool loading_failed = false;
        for (std::size_t i = 0; i < subdirs.size(); ++i)
        {
            auto& subdir = subdirs[i];
            if (!subdir.loaded())
            {
                if (!ctx.offline && mamba::ends_with(subdir.name(), "/noarch"))
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
