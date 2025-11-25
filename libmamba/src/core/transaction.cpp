// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <iostream>
#include <iterator>
#include <ranges>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/download_progress_bar.hpp"
#include "mamba/core/env_lockfile.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_fetcher.hpp"
#include "mamba/core/repo_checker_store.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/util/variant_cmp.hpp"

#include "solver/helpers.hpp"

#include "link.hpp"
#include "progress_bar_impl.hpp"
#include "transaction_context.hpp"

namespace mamba
{
    namespace nl = nlohmann;

    namespace
    {
        bool need_pkg_download(const specs::PackageInfo& pkg_info, MultiPackageCache& caches)
        {
            return caches.get_extracted_dir_path(pkg_info).empty()
                   && caches.get_tarball_path(pkg_info).empty();
        }

        // TODO duplicated function, consider moving it to Pool
        auto database_has_package(solver::libsolv::Database& database, const specs::MatchSpec& spec)
            -> bool
        {
            bool found = false;
            database.for_each_package_matching(
                spec,
                [&](const auto&)
                {
                    found = true;
                    return util::LoopControl::Break;
                }
            );
            return found;
        };

        auto explicit_spec(const specs::PackageInfo& pkg) -> specs::MatchSpec
        {
            auto out = specs::MatchSpec();
            out.set_name(specs::MatchSpec::NameSpec(pkg.name));
            if (!pkg.version.empty())
            {
                out.set_version(
                    specs::VersionSpec::parse(fmt::format("=={}", pkg.version))
                        .or_else([](specs::ParseError&& error) { throw std::move(error); })
                        .value()
                );
            }
            if (!pkg.build_string.empty())
            {
                out.set_build_string(
                    specs::MatchSpec::BuildStringSpec(specs::GlobSpec(pkg.build_string))
                );
            }
            return out;
        }

        auto installed_python(const solver::libsolv::Database& database)
            -> std::optional<specs::PackageInfo>
        {
            // TODO combine Repo and MatchSpec search API in Pool
            auto out = std::optional<specs::PackageInfo>();
            if (auto repo = database.installed_repo())
            {
                database.for_each_package_in_repo(
                    *repo,
                    [&](specs::PackageInfo&& pkg)
                    {
                        if (pkg.name == "python")
                        {
                            out = std::move(pkg);
                            return util::LoopControl::Break;
                        }
                        return util::LoopControl::Continue;
                    }
                );
            }
            return out;
        }

        auto find_python_versions_and_site_packages(
            const solver::Solution& solution,
            const solver::libsolv::Database& database
        ) -> std::pair<std::pair<std::string, std::string>, std::string>
        {
            // We need to find the python version that will be there after this
            // Transaction is finished in order to compile the noarch packages correctly,

            // We need to look into installed packages in case we are not installing a new python
            // version but keeping the current one.
            // Could also be written in term of PrefixData.
            std::string python_site_packages_path = {};
            std::string installed_py_ver = {};
            if (auto pkg = installed_python(database))
            {
                python_site_packages_path = pkg->python_site_packages_path;
                installed_py_ver = pkg->version;
                LOG_INFO << "Found python in installed packages " << installed_py_ver;
            }

            std::string new_py_ver = installed_py_ver;
            if (auto py = solver::find_new_python_in_solution(solution))
            {
                new_py_ver = py->get().version;
                python_site_packages_path = py->get().python_site_packages_path;
            }

            return {
                { std::move(new_py_ver), std::move(installed_py_ver) },
                std::move(python_site_packages_path),
            };
        }
    }

    MTransaction::MTransaction(const CommandParams& command_params, MultiPackageCache& caches)
        : m_multi_cache(caches)
        , m_history_entry(History::UserRequest::prefilled(command_params))
    {
    }

    MTransaction::MTransaction(
        const Context& ctx,
        solver::libsolv::Database& database,
        std::vector<specs::PackageInfo> pkgs_to_remove,
        std::vector<specs::PackageInfo> pkgs_to_install,
        MultiPackageCache& caches
    )
        : MTransaction(ctx.command_params, caches)
    {
        auto not_found = std::stringstream();
        for (const auto& pkg : pkgs_to_remove)
        {
            auto spec = explicit_spec(pkg);
            if (!database_has_package(database, spec))
            {
                not_found << "\n - " << spec.to_string();
            }
        }

        if (auto list = not_found.str(); !list.empty())
        {
            LOG_ERROR << "Could not find packages to remove:" << list << '\n';
            Console::instance().json_write({ { "success", false } });
            throw std::runtime_error("Could not find packages to remove:" + list);
        }

        Console::instance().json_write({ { "success", true } });

        m_requested_specs.reserve(pkgs_to_install.size());
        std::transform(
            pkgs_to_install.begin(),
            pkgs_to_install.end(),
            std::back_insert_iterator(m_requested_specs),
            [](const auto& pkg) { return explicit_spec(pkg); }
        );

        m_history_entry.update.reserve(pkgs_to_install.size());
        for (auto& pkg : pkgs_to_install)
        {
            m_history_entry.update.push_back(explicit_spec(pkg).to_string());
        }
        m_history_entry.remove.reserve(pkgs_to_remove.size());
        for (auto& pkg : pkgs_to_remove)
        {
            m_history_entry.remove.push_back(explicit_spec(pkg).to_string());
        }

        m_solution.actions.reserve(pkgs_to_install.size() + pkgs_to_remove.size());

        std::transform(
            std::move_iterator(pkgs_to_install.begin()),
            std::move_iterator(pkgs_to_install.end()),
            std::back_insert_iterator(m_solution.actions),
            [](specs::PackageInfo&& pkg) { return solver::Solution::Install{ std::move(pkg) }; }
        );

        std::transform(
            std::move_iterator(pkgs_to_remove.begin()),
            std::move_iterator(pkgs_to_remove.end()),
            std::back_insert_iterator(m_solution.actions),
            [](specs::PackageInfo&& pkg) { return solver::Solution::Remove{ std::move(pkg) }; }
        );

        // if no action required, don't even start logging them
        if (!empty())
        {
            Console::instance().json_down("actions");
            Console::instance().json_write({ { "PREFIX", ctx.prefix_params.target_prefix.string() } });
        }

        std::tie(
            m_py_versions,
            m_python_site_packages_path
        ) = find_python_versions_and_site_packages(m_solution, database);
    }

    MTransaction::MTransaction(
        const Context& ctx,
        solver::libsolv::Database& database,
        const solver::Request& request,
        solver::Solution solution,
        MultiPackageCache& caches
    )
        : MTransaction(ctx.command_params, caches)
    {
        const auto& flags = request.flags;
        m_solution = std::move(solution);

        if (flags.keep_user_specs)
        {
            using Request = solver::Request;
            solver::for_each_of<Request::Install, Request::Update>(
                request,
                [&](const auto& item) { m_history_entry.update.push_back(item.spec.to_string()); }
            );
            solver::for_each_of<Request::Remove, Request::Update>(
                request,
                [&](const auto& item) { m_history_entry.remove.push_back(item.spec.to_string()); }
            );
        }
        else
        {
            // The specs to install become all the dependencies of the non intstalled specs
            for (const specs::PackageInfo& pkg : m_solution.packages_to_omit())
            {
                for (const auto& dep : pkg.dependencies)
                {
                    m_history_entry.update.push_back(dep);
                }
            }
        }

        using Request = solver::Request;
        solver::for_each_of<Request::Install, Request::Update>(
            request,
            [&](const auto& item) { m_requested_specs.push_back(item.spec); }
        );

        std::tie(
            m_py_versions,
            m_python_site_packages_path
        ) = find_python_versions_and_site_packages(m_solution, database);

        // if no action required, don't even start logging them
        if (!empty())
        {
            Console::instance().json_down("actions");
            Console::instance().json_write(
                {
                    { "PREFIX", ctx.prefix_params.target_prefix.string() },
                }
            );
        }
    }

    MTransaction::MTransaction(
        const Context& ctx,
        solver::libsolv::Database& database,
        std::vector<specs::PackageInfo> packages,
        MultiPackageCache& caches
    )
        : MTransaction(ctx.command_params, caches)
    {
        LOG_INFO << "MTransaction::MTransaction - packages already resolved (lockfile)";

        m_requested_specs.reserve(packages.size());
        std::transform(
            packages.cbegin(),
            packages.cend(),
            std::back_insert_iterator(m_requested_specs),
            [](const auto& pkg)
            {
                return specs::MatchSpec::parse(
                           fmt::format("{}=={}={}", pkg.name, pkg.version, pkg.build_string)
                )
                    .or_else([](specs::ParseError&& err) { throw std::move(err); })
                    .value();
            }
        );

        m_solution.actions.reserve(packages.size());
        std::transform(
            std::move_iterator(packages.begin()),
            std::move_iterator(packages.end()),
            std::back_insert_iterator(m_solution.actions),
            [](specs::PackageInfo&& pkg) { return solver::Solution::Install{ std::move(pkg) }; }
        );

        std::tie(
            m_py_versions,
            m_python_site_packages_path
        ) = find_python_versions_and_site_packages(m_solution, database);
    }

    class TransactionRollback
    {
    public:

        void record(const UnlinkPackage& unlink)
        {
            m_unlink_stack.push(unlink);
        }

        void record(const LinkPackage& link)
        {
            m_link_stack.push(link);
        }

        void rollback(const Context&)
        {
            while (!m_link_stack.empty())
            {
                m_link_stack.top().undo();
                m_link_stack.pop();
            }

            while (!m_unlink_stack.empty())
            {
                m_unlink_stack.top().undo();
                m_unlink_stack.pop();
            }
        }

    private:

        std::stack<UnlinkPackage> m_unlink_stack;
        std::stack<LinkPackage> m_link_stack;
    };

    bool
    MTransaction::execute(const Context& ctx, ChannelContext& channel_context, PrefixData& prefix)
    {
        // JSON output
        // back to the top level if any action was required
        if (!empty())
        {
            Console::instance().json_up();
        }
        Console::instance().json_write(
            { { "dry_run", ctx.dry_run }, { "prefix", ctx.prefix_params.target_prefix.string() } }
        );
        if (empty())
        {
            Console::instance().json_write(
                { { "message", "All requested packages already installed" } }
            );
        }

        if (ctx.dry_run)
        {
            Console::stream() << "Dry run. Not executing the transaction.";
            return true;
        }

        auto lf = LockFile(ctx.prefix_params.target_prefix / "conda-meta");
        clean_trash_files(ctx.prefix_params.target_prefix, false);

        Console::stream() << "\nTransaction starting";
        fetch_extract_packages(ctx, channel_context);

        if (ctx.download_only)
        {
            Console::stream()
                << "Download only - packages are downloaded and extracted. Skipping the linking phase.";
            return true;
        }

        // Channels coming from the repodata (packages to install) don't have the same channel
        // format than packages coming from the prefix (packages to remove). We set all the channels
        // to be URL like (i.e. explicit). Below is a loop to fix the channel of the linked
        // packages (fix applied to the unlinked packages to avoid potential bugs). Ideally, this
        // should be normalised when reading the data.
        for (specs::PackageInfo& pkg : m_solution.packages())
        {
            const auto unresolved_pkg_channel = mamba::specs::UnresolvedChannel::parse(pkg.channel)
                                                    .value();
            const auto pkg_channel = mamba::specs::Channel::resolve(
                                         unresolved_pkg_channel,
                                         channel_context.params()
            )
                                         .value();
            assert(not pkg_channel.empty());
            const auto channel_url = pkg_channel.front().platform_url(pkg.platform).str();
            pkg.channel = channel_url;

            if (pkg.package_url.empty())
            {
                pkg.package_url = pkg.url_for_channel_platform(channel_url);
            }
        };

        TransactionRollback rollback;
        TransactionContext transaction_context(
            ctx.transaction_params(),
            m_py_versions,
            m_python_site_packages_path,
            m_requested_specs
        );

        for (const specs::PackageInfo& pkg : m_solution.packages_to_remove())
        {
            if (is_sig_interrupted())
            {
                break;
            }
            Console::stream() << "Unlinking " << pkg.str();
            const fs::u8path cache_path(m_multi_cache.get_extracted_dir_path(pkg));
            UnlinkPackage up(pkg, cache_path, &transaction_context);
            up.execute();
            rollback.record(up);
            m_history_entry.unlink_dists.push_back(pkg.long_str());
        }

        for (const specs::PackageInfo& pkg : m_solution.packages_to_install())
        {
            if (is_sig_interrupted())
            {
                break;
            }
            Console::stream() << "Linking " << pkg.str();
            const fs::u8path cache_path(m_multi_cache.get_extracted_dir_path(pkg, false));
            LinkPackage lp(pkg, cache_path, &transaction_context);
            lp.execute();
            rollback.record(lp);
            m_history_entry.link_dists.push_back(pkg.long_str());
        }

        if (is_sig_interrupted())
        {
            Console::stream() << "Transaction interrupted, rollbacking";
            rollback.rollback(ctx);
            return false;
        }
        LOG_INFO << "Waiting for pyc compilation to finish";
        transaction_context.wait_for_pyc_compilation();

        Console::stream() << "\nTransaction finished\n";

        prefix.history().add_entry(m_history_entry);
        return true;
    }

    auto MTransaction::to_conda() -> to_conda_type
    {
        namespace views = std::ranges::views;

        auto to_remove_range = m_solution.packages_to_remove()  //
                               | views::transform(
                                   [](const auto& pkg)
                                   { return to_remove_type::value_type(pkg.channel, pkg.filename); }
                               );
        // TODO(C++23): std::ranges::to
        auto to_remove_structured = to_remove_type(to_remove_range.begin(), to_remove_range.end());

        auto to_install_range = m_solution.packages_to_install()  //
                                | views::transform(
                                    [](const auto& pkg)
                                    {
                                        return to_install_type::value_type(
                                            pkg.channel,
                                            pkg.filename,
                                            nl::json(pkg).dump(4)
                                        );
                                    }
                                );
        // TODO(C++23): std::ranges::to
        auto to_install_structured = to_install_type(to_install_range.begin(), to_install_range.end());

        to_specs_type specs;
        std::get<0>(specs) = m_history_entry.update;
        std::get<1>(specs) = m_history_entry.remove;

        return std::make_tuple(specs, to_install_structured, to_remove_structured);
    }

    void MTransaction::log_json()
    {
        namespace views = std::ranges::views;

        // TODO(C++23): std::ranges::to
        auto to_fetch_range = m_solution.packages_to_install()
                              | views::filter([this](const auto& pkg)
                                              { return need_pkg_download(pkg, m_multi_cache); });
        auto to_fetch = std::vector<nl::json>(to_fetch_range.begin(), to_fetch_range.end());


        // TODO(C++23): std::ranges::to
        auto to_link_range = m_solution.packages_to_install();
        auto to_link = std::vector<nl::json>(to_link_range.begin(), to_link_range.end());

        // TODO(C++23): std::ranges::to
        auto to_unlink_range = m_solution.packages_to_remove();
        auto to_unlink = std::vector<nl::json>(to_unlink_range.begin(), to_unlink_range.end());

        auto add_json = [](const auto& jlist, const char* s)
        {
            if (!jlist.empty())
            {
                Console::instance().json_down(s);
                for (nl::json j : jlist)
                {
                    Console::instance().json_append(j);
                }
                Console::instance().json_up();
            }
        };

        add_json(to_fetch, "FETCH");
        add_json(to_link, "LINK");
        add_json(to_unlink, "UNLINK");
    }

    namespace
    {
        using FetcherList = std::vector<PackageFetcher>;

        // Free functions instead of private method to avoid exposing downloaders
        // and package fetchers in the header. Ideally we may want a pimpl or
        // a private implementation header when we refactor this class.
        FetcherList build_fetchers(
            const Context& ctx,
            ChannelContext& channel_context,
            const solver::Solution& solution,
            MultiPackageCache& multi_cache
        )
        {
            FetcherList fetchers;

            if (ctx.validation_params.verify_artifacts)
            {
                LOG_INFO << "Content trust is enabled, package(s) signatures will be verified";
            }
            for (const auto& pkg : solution.packages_to_install())
            {
                if (ctx.validation_params.verify_artifacts)
                {
                    LOG_INFO << "Creating RepoChecker...";
                    auto repo_checker_store = RepoCheckerStore::make(ctx, channel_context, multi_cache);
                    for (auto& chan : channel_context.make_channel(pkg.channel))
                    {
                        auto repo_checker = repo_checker_store.find_checker(chan);
                        if (repo_checker)
                        {
                            LOG_INFO << "RepoChecker successfully created.";
                            repo_checker->generate_index_checker();
                            repo_checker->verify_package(
                                pkg.json_signable(),
                                std::string_view(pkg.signatures)
                            );
                        }
                        else
                        {
                            LOG_ERROR << "Could not create a valid RepoChecker.";
                            throw std::runtime_error(
                                fmt::format(
                                    R"(Could not verify "{}". Please make sure the package signatures are available and 'trusted-channels' are configured correctly. Alternatively, try downloading without '--verify-artifacts' flag.)",
                                    pkg.name
                                )
                            );
                        }
                    }
                    LOG_INFO << "'" << pkg.name << "' trusted from '" << pkg.channel << "'";
                }

                // FIXME: only do this for micromamba for now
                if (ctx.command_params.is_mamba_exe)
                {
                    using Credentials = typename specs::CondaURL::Credentials;
                    auto l_pkg = pkg;

                    if (!pkg.package_url.empty())
                    {
                        auto channels = channel_context.make_channel(pkg.package_url);
                        assert(channels.size() == 1);  // A URL can only resolve to one channel
                        const auto platform_urls = channels.front().platform_urls();
                        if (!platform_urls.empty())
                        {
                            l_pkg.package_url = platform_urls.front().str(Credentials::Show);
                        }
                    }

                    {
                        auto channels = channel_context.make_channel(pkg.channel);
                        assert(channels.size() == 1);  // A URL can only resolve to one channel
                        const auto& channel = channels.front();

                        l_pkg.channel = channel.id();

                        if (l_pkg.package_url.empty())
                        {
                            l_pkg.package_url = l_pkg.url_for_channel(
                                channel.url().str(Credentials::Show)
                            );
                        }
                    }


                    fetchers.emplace_back(l_pkg, multi_cache);
                }
                else
                {
                    fetchers.emplace_back(pkg, multi_cache);
                }
            }

            if (ctx.validation_params.verify_artifacts)
            {
                auto out = Console::stream();
                fmt::print(
                    out,
                    "Content trust verifications successful, {} ",
                    fmt::styled("package(s) are trusted", ctx.graphics_params.palette.safe)
                );
                LOG_INFO << "All package(s) are trusted";
            }
            return fetchers;
        }

        using ExtractTaskList = std::vector<PackageExtractTask>;

        ExtractTaskList
        build_extract_tasks(const Context& context, FetcherList& fetchers, std::size_t extract_size)
        {
            auto extract_options = ExtractOptions::from_context(context);
            ExtractTaskList extract_tasks;
            extract_tasks.reserve(extract_size);
            std::transform(
                fetchers.begin(),
                fetchers.begin() + static_cast<std::ptrdiff_t>(extract_size),
                std::back_inserter(extract_tasks),
                [extract_options](auto& f) { return f.build_extract_task(extract_options); }
            );
            return extract_tasks;
        }

        using ExtractTrackerList = std::vector<std::future<PackageExtractTask::Result>>;

        download::MultiRequest build_download_requests(
            FetcherList& fetchers,
            ExtractTaskList& extract_tasks,
            ExtractTrackerList& extract_trackers,
            std::size_t download_size
        )
        {
            download::MultiRequest download_requests;
            download_requests.reserve(download_size);
            for (auto [fit, eit] = std::tuple{ fetchers.begin(), extract_tasks.begin() };
                 fit != fetchers.begin() + static_cast<std::ptrdiff_t>(download_size);
                 ++fit, ++eit)
            {
                auto ceit = eit;  // Apple Clang cannot capture eit
                auto task = std::make_shared<std::packaged_task<PackageExtractTask::Result(std::size_t)>>(
                    [ceit](std::size_t downloaded_size) { return ceit->run(downloaded_size); }
                );
                extract_trackers.push_back(task->get_future());
                download_requests.push_back(fit->build_download_request(
                    [extract_task = std::move(task)](std::size_t downloaded_size)
                    {
                        MainExecutor::instance().schedule(
                            [t = std::move(extract_task)](std::size_t ds) { (*t)(ds); },
                            downloaded_size
                        );
                    }
                ));
            }
            return download_requests;
        }

        void schedule_remaining_extractions(
            ExtractTaskList& extract_tasks,
            ExtractTrackerList& extract_trackers,
            std::size_t download_size
        )
        {
            // We schedule extractions for packages that don't need to be downloaded,
            // because downloading a package already triggers its extraction.
            for (auto it = extract_tasks.begin() + static_cast<std::ptrdiff_t>(download_size);
                 it != extract_tasks.end();
                 ++it)
            {
                std::packaged_task<mamba::PackageExtractTask::Result()> task{ [=]
                                                                              { return it->run(); } };
                extract_trackers.push_back(task.get_future());
                MainExecutor::instance().schedule(std::move(task));
            }
        }

        bool trigger_download(
            download::MultiRequest requests,
            const Context& context,
            download::Options options,
            PackageDownloadMonitor* monitor
        )
        {
            auto result = download::download(
                std::move(requests),
                context.mirrors,
                context.remote_fetch_params,
                context.authentication_info(),
                options,
                monitor
            );
            bool all_downloaded = std::all_of(
                result.begin(),
                result.end(),
                [](const auto& r) { return r; }
            );
            return all_downloaded;
        }

        bool clear_invalid_caches(const FetcherList& fetchers, ExtractTrackerList& trackers)
        {
            bool all_valid = true;
            for (auto [fit, eit] = std::tuple{ fetchers.begin(), trackers.begin() };
                 eit != trackers.end();
                 ++fit, ++eit)
            {
                PackageExtractTask::Result res = eit->get();
                if (!res.valid || !res.extracted)
                {
                    fit->clear_cache();
                    all_valid = false;
                }
            }
            return all_valid;
        }
    }

    bool MTransaction::fetch_extract_packages(const Context& ctx, ChannelContext& channel_context)
    {
        PackageFetcherSemaphore::set_max(ctx.threads_params.extract_threads);

        FetcherList fetchers = build_fetchers(ctx, channel_context, m_solution, m_multi_cache);

        auto download_end = std::partition(
            fetchers.begin(),
            fetchers.end(),
            [](const auto& f) { return f.needs_download(); }
        );
        auto extract_end = std::partition(
            download_end,
            fetchers.end(),
            [](const auto& f) { return f.needs_extract(); }
        );

        // At this point:
        // - [fetchers.begin(), download_end) contains packages that need to be downloaded,
        // validated and extracted
        // - [download_end, extract_end) contains packages that need to be extracted only
        // - [extract_end, fetchers.end()) contains packages already installed and extracted

        auto download_size = static_cast<std::size_t>(std::distance(fetchers.begin(), download_end));
        auto extract_size = static_cast<std::size_t>(std::distance(fetchers.begin(), extract_end));

        ExtractTaskList extract_tasks = build_extract_tasks(ctx, fetchers, extract_size);
        ExtractTrackerList extract_trackers;
        extract_trackers.reserve(extract_tasks.size());
        download::MultiRequest download_requests = build_download_requests(
            fetchers,
            extract_tasks,
            extract_trackers,
            download_size
        );

        std::unique_ptr<PackageDownloadMonitor> monitor = nullptr;
        auto download_options = ctx.download_options();
        download_options.fail_fast = true;
        if (PackageDownloadMonitor::can_monitor(ctx))
        {
            monitor = std::make_unique<PackageDownloadMonitor>();
            monitor->observe(download_requests, extract_tasks, download_options);
        }

        schedule_remaining_extractions(extract_tasks, extract_trackers, download_size);
        bool all_downloaded = trigger_download(
            std::move(download_requests),
            ctx,
            download_options,
            monitor.get()
        );
        if (!all_downloaded)
        {
            LOG_ERROR << "Download didn't finish!";
            return false;
        }

        // Blocks until all extraction are done
        for (auto& task : extract_trackers)
        {
            task.wait();
        }

        const bool all_valid = clear_invalid_caches(fetchers, extract_trackers);
        // TODO: see if we can move this into the caller
        if (!all_valid)
        {
            throw std::runtime_error(std::string("Found incorrect downloads. Aborting"));
        }
        return !is_sig_interrupted() && all_valid;
    }

    bool MTransaction::empty()
    {
        return m_solution.actions.empty();
    }

    bool MTransaction::prompt(const Context& ctx, ChannelContext& channel_context)
    {
        print(ctx, channel_context);
        if (ctx.dry_run || empty())
        {
            return true;
        }

        return Console::prompt("Confirm changes", 'y');
    }

    void MTransaction::print(const Context& ctx, ChannelContext& channel_context)
    {
        using Solution = solver::Solution;

        if (ctx.output_params.json)
        {
            return;
        }

        Console::instance().print("Transaction\n");
        Console::stream() << "  Prefix: " << ctx.prefix_params.target_prefix.string() << "\n";

        // check size of transaction
        if (empty())
        {
            if (m_history_entry.update.size())
            {
                Console::instance().print("  All requested packages already installed\n");
            }
            else if (m_history_entry.remove.size())
            {
                // There was no remove events but we still have remove specs treated:
                // The packages to remove were not found in the environment.
                Console::instance().print(
                    "  Failure: packages to remove not found in the environment:\n"
                );
                for (const auto& entry : m_history_entry.remove)
                {
                    Console::instance().print(fmt::format("  - {}\n", entry));
                }
            }
            else
            {
                Console::instance().print("  Nothing to do\n");
            }
            return;
        }

        if (m_history_entry.update.size())
        {
            Console::instance().print("  Updating specs:\n");
            for (auto& s : m_history_entry.update)
            {
                Console::stream() << "   - " << s;
            }
        }

        if (m_history_entry.remove.size())
        {
            Console::instance().print("  Removing specs:\n");
            for (auto& s : m_history_entry.remove)
            {
                Console::stream() << "   - " << s;
            }
        }
        Console::stream() << "\n";
        if (m_history_entry.update.empty() && m_history_entry.remove.empty())
        {
            Console::instance().print("  No specs added or removed.\n");
        }

        printers::Table t({ "Package", "Version", "Build", "Channel", "Size" });
        t.set_alignment(
            { printers::alignment::left,
              printers::alignment::right,
              printers::alignment::left,
              printers::alignment::left,
              printers::alignment::right }
        );
        t.set_padding({ 2, 2, 2, 2, 5 });

        using rows = std::vector<std::vector<printers::FormattedString>>;

        rows downgraded, upgraded, changed, reinstalled, erased, installed, ignored;
        std::size_t total_size = 0;

        enum struct Status
        {
            install,
            ignore,
            remove
        };
        auto format_row = [&](rows& r, const specs::PackageInfo& s, Status status, std::string diff)
        {
            const std::size_t dlsize = s.size;
            printers::FormattedString dlsize_s;
            if (dlsize > 0)
            {
                if (status == Status::ignore)
                {
                    dlsize_s.s = "Ignored";
                }
                else
                {
                    if (!need_pkg_download(s, m_multi_cache))
                    {
                        dlsize_s.s = "Cached";
                        dlsize_s.style = ctx.graphics_params.palette.addition;
                    }
                    else
                    {
                        std::stringstream ss;
                        to_human_readable_filesize(ss, double(dlsize));
                        dlsize_s.s = ss.str();
                        // Hacky hacky
                        if (status == Status::install)
                        {
                            total_size += dlsize;
                        }
                    }
                }
            }
            printers::FormattedString name;
            name.s = fmt::format("{} {}", diff, s.name);
            if (status == Status::install)
            {
                name.style = ctx.graphics_params.palette.addition;
            }
            else if (status == Status::ignore)
            {
                name.style = ctx.graphics_params.palette.ignored;
            }
            else if (status == Status::remove)
            {
                name.style = ctx.graphics_params.palette.deletion;
            }

            std::string chan_name;
            if (auto str = s.channel; !str.empty())
            {
                if (str == "explicit_specs")
                {
                    chan_name = s.filename;
                }
                else
                {
                    auto channels = channel_context.make_channel(str);
                    if (channels.size() == 1)
                    {
                        chan_name = channels.front().display_name();
                    }
                    else
                    {
                        // If there is more than on, it's a custom_multi_channel name
                        // This should never happen
                        chan_name = str;
                    }
                }
            }
            else
            {
                // note this can and should be <unknown> when e.g. installing from a tarball
                chan_name = s.channel;
                assert(chan_name != "__explicit_specs__");
            }

            r.push_back(
                { name,
                  printers::FormattedString(s.version),
                  printers::FormattedString(s.build_string),
                  printers::FormattedString(cut_repo_name(chan_name)),
                  dlsize_s }
            );
        };

        auto format_action = [&](const auto& act)
        {
            using Action = std::decay_t<decltype(act)>;
            if constexpr (std::is_same_v<Action, Solution::Omit>)
            {
                format_row(ignored, act.what, Status::ignore, "=");
            }
            else if constexpr (std::is_same_v<Action, Solution::Upgrade>)
            {
                format_row(upgraded, act.remove, Status::remove, "-");
                format_row(upgraded, act.install, Status::install, "+");
            }
            else if constexpr (std::is_same_v<Action, Solution::Downgrade>)
            {
                format_row(downgraded, act.remove, Status::remove, "-");
                format_row(downgraded, act.install, Status::install, "+");
            }
            else if constexpr (std::is_same_v<Action, Solution::Change>)
            {
                format_row(changed, act.remove, Status::remove, "-");
                format_row(changed, act.install, Status::install, "+");
            }
            else if constexpr (std::is_same_v<Action, Solution::Reinstall>)
            {
                format_row(reinstalled, act.what, Status::install, "o");
            }
            else if constexpr (std::is_same_v<Action, Solution::Remove>)
            {
                format_row(erased, act.remove, Status::remove, "-");
            }
            else if constexpr (std::is_same_v<Action, Solution::Install>)
            {
                format_row(installed, act.install, Status::install, "+");
            }
        };

        // Sort actions to print by type first and package name second.
        // The type does not really influence anything since they are later grouped together.
        // In the absence of a better/alternative solution, such as a tree view of install
        // requirements, this is more readable than the Solution's order.
        // WARNING: do not sort the solution as it is topologically sorted for installing
        // dependencies before dependent.
        auto actions = m_solution.actions;
        std::sort(
            actions.begin(),
            actions.end(),
            util::make_variant_cmp(
                /* index_cmp= */
                [](auto lhs, auto rhs) { return lhs < rhs; },
                /* alternative_cmp= */
                [](const auto& lhs, const auto& rhs)
                {
                    using Action = std::decay_t<decltype(lhs)>;  // rhs has same type.
                    if constexpr (solver::Solution::has_remove_v<Action>)
                    {
                        return lhs.remove.name < rhs.remove.name;
                    }
                    else if constexpr (solver::Solution::has_install_v<Action>)
                    {
                        return lhs.install.name < rhs.install.name;
                    }
                    else
                    {
                        return lhs.what.name < rhs.what.name;
                    }
                }
            )
        );

        for (const auto& pkg : actions)
        {
            std::visit(format_action, pkg);
        }

        std::stringstream summary;
        summary << "Summary:\n\n";
        if (installed.size())
        {
            t.add_rows("Install:", installed);
            summary << "  Install: " << installed.size() << " packages\n";
        }
        if (erased.size())
        {
            t.add_rows("Remove:", erased);
            summary << "  Remove: " << erased.size() << " packages\n";
        }
        if (changed.size())
        {
            t.add_rows("Change:", changed);
            summary << "  Change: " << changed.size() / 2 << " packages\n";
        }
        if (reinstalled.size())
        {
            t.add_rows("Reinstall:", reinstalled);
            summary << "  Reinstall: " << reinstalled.size() << " packages\n";
        }
        if (upgraded.size())
        {
            t.add_rows("Upgrade:", upgraded);
            summary << "  Upgrade: " << upgraded.size() / 2 << " packages\n";
        }
        if (downgraded.size())
        {
            t.add_rows("Downgrade:", downgraded);
            summary << "  Downgrade: " << downgraded.size() / 2 << " packages\n";
        }
        if (ignored.size())
        {
            t.add_rows("Ignored:", ignored);
            summary << "  Ignored: " << ignored.size() << " packages\n";
        }

        summary << "\n  Total download: ";
        to_human_readable_filesize(summary, double(total_size));
        summary << "\n";
        t.add_row({ summary.str() });
        auto out = Console::stream();
        t.print(out);
    }

    MTransaction create_explicit_transaction_from_urls(
        const Context& ctx,
        solver::libsolv::Database& database,
        const std::vector<std::string>& urls,
        MultiPackageCache& package_caches,
        std::vector<detail::other_pkg_mgr_spec>&
    )
    {
        std::vector<specs::PackageInfo> specs_to_install = {};
        specs_to_install.reserve(urls.size());
        std::transform(
            urls.cbegin(),
            urls.cend(),
            std::back_insert_iterator(specs_to_install),
            [&](const auto& u)
            {
                return specs::PackageInfo::from_url(u)
                    .or_else([](specs::ParseError&& err) { throw std::move(err); })
                    .value();
            }
        );
        return MTransaction(ctx, database, {}, specs_to_install, package_caches);
    }

    MTransaction create_explicit_transaction_from_lockfile(
        const Context& ctx,
        solver::libsolv::Database& database,
        const fs::u8path& env_lockfile_path,
        const std::vector<std::string>& categories,
        MultiPackageCache& package_caches,
        std::vector<detail::other_pkg_mgr_spec>& other_specs
    )
    {
        const auto maybe_lockfile = read_environment_lockfile(env_lockfile_path);
        if (!maybe_lockfile)
        {
            throw maybe_lockfile.error();  // NOTE: we cannot return an `un/expected` because
                                           // MTransaction is not move-enabled.
        }

        const auto lockfile_data = maybe_lockfile.value();

        for (const auto& package : lockfile_data.get_all_packages())
        {
            LOG_DEBUG << "parsed package: " << package.info.name;
            LOG_DEBUG << "  category = " << package.category;
            LOG_DEBUG << "  platform = " << package.platform;
            LOG_DEBUG << "  manager = " << package.manager;
        }

        // TODO: FIXME: inject channel info coming from the lockfile!

        std::vector<specs::PackageInfo> conda_packages = {};
        std::vector<specs::PackageInfo> pip_packages = {};

        for (const auto& category : categories)
        {
            std::vector<specs::PackageInfo> selected_packages = lockfile_data.get_packages_for(
                { .category = category, .platform = ctx.platform, .manager = "conda" }
            );
            std::copy(
                selected_packages.begin(),
                selected_packages.end(),
                std::back_inserter(conda_packages)
            );

            if (selected_packages.empty())
            {
                LOG_WARNING << "Selected packages for category '" << category << "' are empty. "
                            << "The lockfile might not be resolved for your platform ("
                            << ctx.platform << ").";
            }

            selected_packages = lockfile_data.get_packages_for(
                { .category = category,
                  .platform = ctx.platform,
                  .manager = "pip",
                  // NOTE: sometime python packages can have no platform specified (mambajs lockfile
                  // for
                  //       example) in this case we just take the package if not specified, but if
                  //       specified we filter to the current platform.
                  .allow_no_platform = true }
            );
            std::copy(
                selected_packages.begin(),
                selected_packages.end(),
                std::back_inserter(pip_packages)
            );
        }

        // extract pip packages
        if (!pip_packages.empty())
        {
            std::vector<std::string> pip_specs = {};
            pip_specs.reserve(pip_packages.size());
            std::transform(
                pip_packages.cbegin(),
                pip_packages.cend(),
                std::back_inserter(pip_specs),
                [](const specs::PackageInfo& pkg)
                { return fmt::format("{} @ {}#sha256={}", pkg.name, pkg.package_url, pkg.sha256); }
            );
            other_specs.push_back(
                { "pip --no-deps", pip_specs, fs::absolute(env_lockfile_path.parent_path()).string() }
            );
        }


        for (const auto& package : pip_packages)
        {
            LOG_DEBUG << "pip package to install: " << package.name;
        }

        return MTransaction{ ctx, database, std::move(conda_packages), package_caches };
    }

}  // namespace mamba
