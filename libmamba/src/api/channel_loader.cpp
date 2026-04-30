// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <chrono>
#include <optional>
#include <ranges>
#include <set>
#include <sstream>
#include <unordered_set>

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
#include "mamba/specs/error.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/specs/version.hpp"
#include "mamba/util/string.hpp"

#include "utils.hpp"

namespace mamba
{
    namespace
    {
        auto shorten_status_label(std::string label) -> std::string
        {
            if (label.length() > 85)
            {
                label = label.substr(0, 82) + "...";
            }
            return label;
        }

        auto done_with_duration(std::chrono::steady_clock::duration elapsed) -> std::string
        {
            const double seconds = std::chrono::duration<double>(elapsed).count();
            return fmt::format("✔ Done ({:.1f} sec)", seconds);
        }

        void print_status_line(std::string label, const std::string& status, bool finalize = false)
        {
            if (!Console::can_report_status())
            {
                return;
            }
            Console::instance().print_in_place(
                fmt::format("{:<85} {:>20}", shorten_status_label(std::move(label)), status),
                finalize
            );
        }

        void print_done_status_line(
            std::string label,
            std::optional<std::chrono::steady_clock::duration> elapsed = std::nullopt
        )
        {
            print_status_line(
                std::move(label),
                elapsed.has_value() ? done_with_duration(*elapsed) : std::string("✔ Done"),
                true
            );
        }

        void
        print_flat_repodata_done(const SubdirIndexLoader& subdir, std::chrono::steady_clock::duration elapsed)
        {
            print_done_status_line("Using Flat Repodata for " + subdir.name(), elapsed);
        }

        void print_flat_repodata_start(const SubdirIndexLoader& subdir)
        {
            print_status_line("Using Flat Repodata for " + subdir.name(), "✔ Starting");
        }

        void print_parsing_records_done(std::chrono::steady_clock::duration elapsed)
        {
            print_done_status_line("Parsing Packages' Records", elapsed);
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

        auto build_packages_by_url_from_subset(
            const RepodataSubset& subset,
            const std::vector<SubdirIndexLoader>& subdirs,
            const std::map<std::string, std::size_t>& url_to_subdir_idx
        ) -> expected_t<std::map<std::string, std::vector<specs::PackageInfo>>>
        {
            std::map<std::string, std::vector<specs::PackageInfo>> packages_by_url;
            bool had_shard_error = false;
            for (const auto& [node_id, node] : subset.nodes())
            {
                if (!node.visited)
                {
                    continue;
                }

                auto it = std::find_if(
                    subset.shards().begin(),
                    subset.shards().end(),
                    [&node_id](const Shards& s) { return s.url() == node_id.channel; }
                );
                if (it == subset.shards().end())
                {
                    continue;
                }
                const Shards& shards = *it;
                try
                {
                    auto shard = shards.visit_package(node_id.package);
                    auto base_url = shards.base_url();
                    specs::DynamicPlatform platform = shards.subdir();
                    std::string channel_id = subdirs[url_to_subdir_idx.at(node_id.channel)].channel_id();
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
                catch (const std::exception& e)
                {
                    had_shard_error = true;
                    LOG_WARNING << "Failed to load package " << node_id.package << " from "
                                << node_id.channel << ": " << e.what();
                }
            }
            if (had_shard_error)
            {
                return make_unexpected(
                    "At least one shard failed to load from the reachable subset",
                    mamba_error_code::subdirdata_not_loaded
                );
            }
            return { std::move(packages_by_url) };
        }

        // Forward declarations for helpers defined later in this namespace.
        void create_mirrors(const specs::Channel& channel, download::mirror_map& mirrors);

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
        );

        /**
         * Prepare subdir loaders, priorities, and direct package URLs.
         *
         * This function:
         *   - expands mirrored and regular channels into concrete channels,
         *   - configures mirrors and creates `SubdirIndexLoader`s,
         *   - computes priorities for each subdir,
         *   - collects any channel-as-package URLs into `packages`,
         *   - appends loader creation errors into `error_list`.
         */
        void prepare_subdirs_and_packages(
            Context& ctx,
            ChannelContext& channel_context,
            MultiPackageCache& package_caches,
            std::vector<SubdirIndexLoader>& subdirs,
            std::vector<solver::libsolv::Priorities>& priorities,
            std::vector<specs::PackageInfo>& packages,
            std::vector<mamba_error>& error_list
        )
        {
            int max_prio = static_cast<int>(ctx.channels.size());
            specs::CondaURL previous_channel_url;

            // Process mirrored channels: create channel objects, configure mirrors, and initialize
            // subdirs for each platform.
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

            // Process regular (non-mirrored) channels.
            for (const auto& location : ctx.channels)
            {
                if (ctx.mirrored_channels.contains(location))
                {
                    continue;
                }

                for (const specs::Channel& channel : channel_context.make_channel(location))
                {
                    if (channel.is_package())
                    {
                        specs::PackageInfo pkg_info = specs::PackageInfo::from_url(channel.url().str())
                                                          .or_else([](specs::ParseError&& err)
                                                                   { throw std::move(err); })
                                                          .value();
                        packages.push_back(pkg_info);
                    }
                    else
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
            }
        }

        /**
         * Load a single subdir into the database, using shards when available.
         *
         * When shards are enabled and up to date, this:
         *   - attempts ``load_subdir_with_shards``,
         *   - falls back to cached full repodata when shard loading fails,
         *   - optionally fetches fresh full ``repodata.json`` and loads it if there is no cache.
         * Subdirs without a shard index load full ``repodata.json`` directly.
         *
         * When ``use_sharded_repodata`` is false, or ``root_packages`` is empty, loads full
         * repodata (or native solv cache) as usual.
         */
        expected_t<solver::libsolv::RepoInfo> load_single_subdir(
            Context& ctx,
            solver::libsolv::Database& database,
            std::vector<std::string>& root_packages,
            std::vector<SubdirIndexLoader>& subdirs,
            std::size_t subdir_idx,
            std::set<std::string>& loaded_subdirs_with_shards,
            const SubdirDownloadParams& subdir_params,
            const std::vector<solver::libsolv::Priorities>& priorities,
            std::optional<specs::Version> python_minor_version_for_prefilter,
            bool* used_flat_repodata,
            std::optional<std::chrono::steady_clock::time_point>* flat_repodata_started_at
        )
        {
            auto& subdir = subdirs[subdir_idx];
            const bool shards_requested = ctx.use_sharded_repodata && !root_packages.empty();
            const auto load_flat_repodata_with_status = [&]() -> expected_t<solver::libsolv::RepoInfo>
            {
                if (shards_requested && !subdir.metadata().has_shards())
                {
                    Console::instance().print(
                        "⚠ Shard Index for " + subdir.name()
                        + " not available, falling back to flat repodata"
                    );
                }
                const auto started_at = std::chrono::steady_clock::now();
                if (flat_repodata_started_at != nullptr && !flat_repodata_started_at->has_value())
                {
                    *flat_repodata_started_at = started_at;
                }
                print_flat_repodata_start(subdir);
                auto flat_res = load_subdir_in_database(ctx, database, subdir);
                if (flat_res)
                {
                    if (used_flat_repodata != nullptr)
                    {
                        *used_flat_repodata = true;
                    }
                    print_flat_repodata_done(subdir, std::chrono::steady_clock::now() - started_at);
                }
                return flat_res;
            };

            bool use_shards = ctx.use_sharded_repodata
                              && subdir.metadata().has_up_to_date_shards(ctx.repodata_shards_ttl)
                              && !root_packages.empty();

            if (use_shards)
            {
                auto res = load_subdir_with_shards(
                    ctx,
                    database,
                    root_packages,
                    subdirs,
                    subdir_idx,
                    loaded_subdirs_with_shards,
                    priorities,
                    python_minor_version_for_prefilter
                );

                if (!res)
                {
                    if (subdir.valid_cache_found())
                    {
                        return load_flat_repodata_with_status();
                    }
                    // Shards failed and no cache - try to fetch and load flat repodata.
                    if (!ctx.offline)
                    {
                        LOG_DEBUG << "Shard loading failed for " << subdir.name()
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
                            return load_flat_repodata_with_status();
                        }
                    }
                }
                return res;
            }

            return load_flat_repodata_with_status();
        }

        /**
         * Download and refresh repodata indexes for all relevant subdirs.
         *
         * Steps:
         *   1. Run lightweight HEAD checks for all subdirs (records errors, aborts on
         *      user interruption).
         *   2. Identify subdirs that still require full repodata indexes (i.e. not using
         *      shards for the current `root_packages`).
         *   3. Download full indexes for those subdirs, again propagating user interruption
         *      and adding recoverable errors to `error_list`.
         *
         * Returns an unexpected `mamba_aggregated_error` only when a user interrupt occurs;
         * other errors are accumulated in `error_list`.
         */
        expected_t<void, mamba_aggregated_error> download_subdir_indexes(
            Context& ctx,
            const std::vector<std::string>& root_packages,
            std::vector<SubdirIndexLoader>& subdirs,
            const SubdirDownloadParams& subdir_params,
            std::vector<mamba_error>& error_list
        )
        {
            SubdirIndexMonitor check_monitor({ true, true });

            auto check_res = SubdirIndexLoader::download_requests(
                SubdirIndexLoader::build_all_check_requests(subdirs.begin(), subdirs.end(), subdir_params),
                ctx.authentication_info(),
                ctx.mirrors,
                ctx.download_options(),
                ctx.remote_fetch_params,
                SubdirIndexMonitor::can_monitor(ctx) ? &check_monitor : nullptr
            );

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

            // Ensure shards availability is set from cache for all subdirs. This complements
            // network shard checks and still avoids full repodata downloads when shard index cache
            // is within TTL.
            for (auto& s : subdirs)
            {
                s.maybe_set_shards_from_cache(subdir_params);
            }

            // Collect only subdirs that still need full repodata indexes.
            std::vector<SubdirIndexLoader*> subdirs_needing_index;
            for (auto& s : subdirs)
            {
                bool use_shards = ctx.use_sharded_repodata
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

            return expected_t<void, mamba_aggregated_error>();
        }

        /**
         * Add repos from local `pkgs_dir` when operating in offline mode.
         *
         * This is a no-op unless `ctx.offline` is true. In that case, it iterates over
         * all configured `pkgs_dirs` and calls `create_repo_from_pkgs_dir` for each.
         */
        void add_repos_from_pks_dir(
            Context& ctx,
            ChannelContext& channel_context,
            solver::libsolv::Database& database
        )
        {
            if (!ctx.offline)
            {
                return;
            }

            LOG_INFO << "Creating repo from pkgs_dir for offline";
            for (const auto& c : ctx.pkgs_dirs)
            {
                create_repo_from_pkgs_dir(ctx, channel_context, database, c);
            }
        }

        /**
         * Extend ``root_packages`` with dependency (and constrain) names reachable from current
         * roots via repos loaded from full repodata (e.g. labels without shards), using a name BFS
         * so the whole subdir is not scanned. Feeds shard BFS on sharded channels (e.g.
         * conda-forge). For example, ``conda-forge/label/mamba_prerelease`` has no
         * ``repodata_shards`` on anaconda.org while main ``conda-forge`` is sharded; prerelease
         * ``mamba`` / ``libmamba`` there may depend on e.g. ``libmsgpack-c`` resolved from shards,
         * which roots like ``mamba`` alone would not reach without this pass.
         */
        void expand_shard_root_packages_from_full_repodata_repos(
            const solver::libsolv::Database& database,
            const std::vector<solver::libsolv::RepoInfo>& full_repos,
            std::vector<std::string>& root_packages
        )
        {
            std::unordered_set<std::string> seen(root_packages.begin(), root_packages.end());
            std::vector<std::string> frontier(root_packages.begin(), root_packages.end());
            auto add_from_spec = [&](const std::string& dep_str)
            {
                if (auto name = specs::MatchSpec::extract_name(dep_str))
                {
                    if (!name->empty() && *name != "*")
                    {
                        if (seen.insert(*name).second)
                        {
                            frontier.push_back(*name);
                            root_packages.push_back(*name);
                        }
                    }
                }
            };

            while (!frontier.empty())
            {
                const std::string pkg_name = std::move(frontier.back());
                frontier.pop_back();

                for (const auto& repo : full_repos)
                {
                    database.for_each_package_in_repo(
                        repo,
                        [&](const specs::PackageInfo& pkg)
                        {
                            if (pkg.name != pkg_name)
                            {
                                return;
                            }
                            for (const auto& dep : pkg.dependencies)
                            {
                                add_from_spec(dep);
                            }
                            for (const auto& c : pkg.constrains)
                            {
                                add_from_spec(c);
                            }
                        }
                    );
                }
            }
        }

        void expand_shard_root_packages_from_packages_by_url(
            const std::map<std::string, std::vector<specs::PackageInfo>>& packages_by_url,
            std::vector<std::string>& root_packages
        )
        {
            std::unordered_set<std::string> seen(root_packages.begin(), root_packages.end());
            auto add_from_spec = [&](const std::string& dep_str)
            {
                if (auto name = specs::MatchSpec::extract_name(dep_str); name)
                {
                    if (!name->empty() && *name != "*" && seen.insert(*name).second)
                    {
                        root_packages.push_back(*name);
                    }
                }
            };

            std::ranges::for_each(
                packages_by_url | std::views::values,
                [&](const std::vector<specs::PackageInfo>& packages)
                {
                    std::ranges::for_each(
                        packages,
                        [&](const specs::PackageInfo& pkg)
                        {
                            std::ranges::for_each(pkg.dependencies, add_from_spec);
                            std::ranges::for_each(pkg.constrains, add_from_spec);
                        }
                    );
                }
            );
        }

        /**
         * Load all subdirs into the database, with a single retry on cache corruption.
         *
         * When sharded repodata is enabled with non-empty ``root_packages``, subdirs that load from
         * full repodata (no shard index) run first; roots are expanded from those repos so shard
         * loads on other channels stay complete. Otherwise a single pass is used.
         *
         * For each `SubdirIndexLoader`, this:
         *   - skips subdirs already loaded via shards,
         *   - verifies cache or skips when cache is missing and shards are not used,
         *   - delegates actual loading (shards vs. full repodata) to
         *     `load_single_subdir`,
         *   - on first failure, clears the cache and marks `loading_failed` for a later retry,
         *   - on second failure (`is_retry == true`), records a hard error in `error_list`.
         *
         * @return true if at least one subdir requested a retry (cache cleared), false otherwise.
         */
        bool load_all_subdirs(
            Context& ctx,
            solver::libsolv::Database& database,
            std::vector<std::string>& root_packages,
            std::vector<SubdirIndexLoader>& subdirs,
            const std::vector<solver::libsolv::Priorities>& priorities,
            const SubdirDownloadParams& subdir_params,
            bool is_retry,
            std::vector<mamba_error>& error_list,
            std::optional<specs::Version> python_minor_version_for_prefilter
        )
        {
            std::set<std::string> loaded_subdirs_with_shards;
            bool loading_failed = false;
            const bool shard_then_expand = ctx.use_sharded_repodata && !root_packages.empty();
            std::size_t roots_after_full_repodata_pass = root_packages.size();
            std::vector<solver::libsolv::RepoInfo> full_repos_for_shard_roots;
            bool used_flat_repodata = false;
            std::optional<std::chrono::steady_clock::time_point> flat_repodata_started_at;

            auto try_load = [&](std::size_t i, bool full_repodata_only_pass) -> void
            {
                auto& subdir = subdirs[i];
                bool use_shards = ctx.use_sharded_repodata
                                  && subdir.metadata().has_up_to_date_shards(ctx.repodata_shards_ttl)
                                  && !root_packages.empty();

                if (full_repodata_only_pass && use_shards)
                {
                    return;
                }
                if (!full_repodata_only_pass && shard_then_expand && !use_shards)
                {
                    return;
                }

                // Skip if this subdir was already loaded as part of a sharded same-channel load.
                if (loaded_subdirs_with_shards.contains(subdir.name()))
                {
                    return;
                }

                // When using shards we don't require valid cache here; the shard path may load
                // anyway.
                if (!subdir.valid_cache_found() && !use_shards)
                {
                    if (!ctx.offline && subdir.is_noarch())
                    {
                        error_list.push_back(mamba_error(
                            "Subdir " + subdir.name() + " not loaded!",
                            mamba_error_code::subdirdata_not_loaded
                        ));
                    }
                    return;
                }

                auto result = load_single_subdir(
                    ctx,
                    database,
                    root_packages,
                    subdirs,
                    i,
                    loaded_subdirs_with_shards,
                    subdir_params,
                    priorities,
                    python_minor_version_for_prefilter,
                    &used_flat_repodata,
                    &flat_repodata_started_at
                );

                if (result)
                {
                    auto repo = std::move(result).value();
                    // `load_subdir_with_shards` already sets priorities for all repos it adds.
                    // Avoid overriding another repo when this subdir has no direct match.
                    if (!use_shards)
                    {
                        database.set_repo_priority(repo, priorities[i]);
                    }
                    if (shard_then_expand && full_repodata_only_pass && !use_shards)
                    {
                        full_repos_for_shard_roots.push_back(repo);
                    }
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
            };

            if (shard_then_expand)
            {
                const std::size_t roots_before = root_packages.size();
                for (std::size_t i = 0; i < subdirs.size(); ++i)
                {
                    try_load(i, /*full_repodata_only_pass=*/true);
                }
                expand_shard_root_packages_from_full_repodata_repos(
                    database,
                    full_repos_for_shard_roots,
                    root_packages
                );
                if (root_packages.size() > roots_before)
                {
                    LOG_DEBUG << "Shard root packages expanded by "
                              << (root_packages.size() - roots_before)
                              << " name(s) from full-repodata subdirs (cross-channel closure seeds).";
                }
                roots_after_full_repodata_pass = root_packages.size();
            }

            for (std::size_t i = 0; i < subdirs.size(); ++i)
            {
                try_load(i, /*full_repodata_only_pass=*/false);
            }

            if (shard_then_expand && root_packages.size() > roots_after_full_repodata_pass)
            {
                LOG_DEBUG << "Shard root packages expanded by "
                          << (root_packages.size() - roots_after_full_repodata_pass)
                          << " additional name(s) during shard pass; re-running shard pass once.";
                loaded_subdirs_with_shards.clear();
                for (std::size_t i = 0; i < subdirs.size(); ++i)
                {
                    try_load(i, /*full_repodata_only_pass=*/false);
                }
            }

            if (used_flat_repodata)
            {
                const auto started_at = flat_repodata_started_at.value_or(
                    std::chrono::steady_clock::now()
                );
                print_parsing_records_done(std::chrono::steady_clock::now() - started_at);
            }

            return loading_failed;
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

        std::optional<solver::libsolv::RepoInfo> add_repos_from_packages_by_url(
            Context& ctx,
            solver::libsolv::Database& database,
            const std::map<std::string, std::vector<specs::PackageInfo>>& packages_by_url,
            std::vector<SubdirIndexLoader>& subdirs,
            const std::map<std::string, std::size_t>& url_to_subdir_idx,
            const std::vector<solver::libsolv::Priorities>& priorities,
            const std::string& current_repodata_url,
            std::set<std::string>& loaded_subdirs_with_shards
        )
        {
            std::optional<solver::libsolv::RepoInfo> result_repo;
            for (const auto& [channel_url, pkgs] : packages_by_url)
            {
                std::string repo_name = subdirs.at(url_to_subdir_idx.at(channel_url)).name();
                if (loaded_subdirs_with_shards.contains(repo_name))
                {
                    continue;
                }
                loaded_subdirs_with_shards.insert(repo_name);

                auto sorted_pkgs = pkgs;
                specs::sort_packages_by_version_and_build_desc(sorted_pkgs);
                auto repo = database.add_repo_from_packages(
                    sorted_pkgs,
                    repo_name,
                    solver::libsolv::PipAsPythonDependency(ctx.add_pip_as_python_dependency)
                );
                std::size_t idx = url_to_subdir_idx.at(channel_url);
                database.set_repo_priority(repo, priorities[idx]);
                if (!result_repo.has_value() || channel_url == current_repodata_url)
                {
                    result_repo = std::move(repo);
                }
            }
            return result_repo;
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

    }  // anonymous namespace

    auto load_subdir_with_shards(
        Context& ctx,
        solver::libsolv::Database& database,
        std::vector<std::string>& root_packages,
        std::vector<SubdirIndexLoader>& subdirs,
        std::size_t subdir_idx,
        std::set<std::string>& loaded_subdirs_with_shards,
        const std::vector<solver::libsolv::Priorities>& priorities,
        std::optional<specs::Version> python_minor_version_for_prefilter
    ) -> expected_t<solver::libsolv::RepoInfo>
    {
        auto& subdir = subdirs[subdir_idx];
        LOG_DEBUG << "Attempting to load subdir with shards: " << subdir.name();

        // Fetch and parse the shard index for the requested subdir (subdir_idx).
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
            LOG_WARNING << "Failed to fetch shard index for " << subdir.name();
            return tl::unexpected(mamba_error(
                "Failed to fetch shard index for " + subdir.name(),
                mamba_error_code::subdirdata_not_loaded
            ));
        }

        LOG_DEBUG << "Shard index fetched for " << subdir.name();
        const auto& channel = subdir.channel();
        std::string current_repodata_url = subdir.repodata_url().str();
        if (python_minor_version_for_prefilter.has_value())
        {
            LOG_DEBUG << "Shard prefilter on python minor version enabled with "
                      << python_minor_version_for_prefilter.value().to_string();
        }
        else
        {
            LOG_DEBUG << "Shard prefilter on python minor version disabled.";
        }

        // For all subdirs sharing the same channel URL, fetch their shard indices and build
        //    a Shards instance per subdir; collect them into a RepodataSubset.
        std::vector<Shards> all_shards;
        std::map<std::string, std::size_t> url_to_subdir_idx;
        for (std::size_t j = 0; j < subdirs.size(); ++j)
        {
            if (subdirs[j].channel().url() == channel.url())
            {
                auto sidx_result = (j == subdir_idx) ? std::move(shard_index_result)
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
                all_shards.emplace_back(
                    std::move(si),
                    sdir_url,
                    subdirs[j].channel(),
                    ctx.authentication_info(),
                    ctx.remote_fetch_params,
                    normalize_to_affinity_concurrency(static_cast<int>(ctx.repodata_shards_threads)),
                    std::cref(ctx.mirrors),
                    python_minor_version_for_prefilter
                );
                url_to_subdir_idx[sdir_url] = j;
            }
        }

        // Compute the reachable subset from root_packages (BFS) so only needed shards
        //    are considered.
        RepodataSubset subset(std::move(all_shards));
        subset.reachable(root_packages, "bfs", std::nullopt);
        LOG_DEBUG << "Reachable subset computed for " << subdir.name() << " ("
                  << subset.shards().size() << " shards, " << subset.nodes().size() << " nodes)";

        // For each visited node in the subset, visit the corresponding package shard and
        //    convert records to PackageInfo; collect packages by channel URL. Exceptions
        //    from individual packages are logged and skipped.
        auto packages_by_url = build_packages_by_url_from_subset(subset, subdirs, url_to_subdir_idx);
        if (!packages_by_url)
        {
            return tl::unexpected(mamba_error(
                "Failed to build package list from shards for " + subdir.name(),
                mamba_error_code::subdirdata_not_loaded
            ));
        }
        const std::size_t roots_before = root_packages.size();
        expand_shard_root_packages_from_packages_by_url(packages_by_url.value(), root_packages);
        if (root_packages.size() > roots_before)
        {
            LOG_DEBUG << "Shard root packages expanded by " << (root_packages.size() - roots_before)
                      << " name(s) from shard-loaded package metadata.";
        }

        // For each channel URL with packages, add a repo to database (unless already in
        //    loaded_subdirs_with_shards), sort packages by version/build, and set repo
        //    priority. The repo for the requested subdir's repodata URL is returned on success.
        std::optional<solver::libsolv::RepoInfo> result_repo = add_repos_from_packages_by_url(
            ctx,
            database,
            packages_by_url.value(),
            subdirs,
            url_to_subdir_idx,
            priorities,
            current_repodata_url,
            loaded_subdirs_with_shards
        );
        // If no packages were loaded for the requested subdir, return an error.
        if (result_repo)
        {
            LOG_INFO << "Loaded subdir with shards: " << subdir.name();
            return std::move(*result_repo);
        }
        LOG_DEBUG << "No packages loaded from shards for " << subdir.name();
        return tl::unexpected(
            mamba_error("No packages for " + subdir.name(), mamba_error_code::subdirdata_not_loaded)
        );
    }

    namespace
    {

        expected_t<void, mamba_aggregated_error> load_channels_impl(
            Context& ctx,
            ChannelContext& channel_context,
            solver::libsolv::Database& database,
            MultiPackageCache& package_caches,
            const std::vector<std::string>& root_packages,
            bool is_retry,
            std::optional<specs::Version> python_minor_version_for_prefilter
        )
        {
            std::vector<SubdirIndexLoader> subdirs;
            std::vector<solver::libsolv::Priorities> priorities;
            Console::instance().init_progress_bar_manager(ProgressBarMode::multi);

            std::vector<mamba_error> error_list;
            std::vector<specs::PackageInfo> packages;
            prepare_subdirs_and_packages(
                ctx,
                channel_context,
                package_caches,
                subdirs,
                priorities,
                packages,
                error_list
            );

            if (!packages.empty())
            {
                database.add_repo_from_packages(packages, "packages");
            }

            auto subdir_params = ctx.subdir_download_params();
            auto download_status = download_subdir_indexes(
                ctx,
                root_packages,
                subdirs,
                subdir_params,
                error_list
            );
            if (!download_status)
            {
                return download_status;
            }

            add_repos_from_pks_dir(ctx, channel_context, database);

            std::vector<std::string> effective_shard_root_packages(root_packages);
            bool loading_failed = load_all_subdirs(
                ctx,
                database,
                effective_shard_root_packages,
                subdirs,
                priorities,
                subdir_params,
                is_retry,
                error_list,
                python_minor_version_for_prefilter
            );

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
                        retry,
                        python_minor_version_for_prefilter
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
        const std::vector<std::string>& root_packages,
        std::optional<specs::Version> python_minor_version_for_prefilter
    ) -> expected_t<void, mamba_aggregated_error>
    {
        bool retry = false;
        return load_channels_impl(
            ctx,
            channel_context,
            database,
            package_caches,
            root_packages,
            retry,
            std::move(python_minor_version_for_prefilter)
        );
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
