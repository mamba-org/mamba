// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <iostream>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <solv/selection.h>
#include <solv/solver.h>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/download_progress_bar.hpp"
#include "mamba/core/env_lockfile.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/link.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_fetcher.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/variant_cmp.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/queue.hpp"
#include "solv-cpp/repo.hpp"
#include "solv-cpp/transaction.hpp"

#include "solver/helpers.hpp"
#include "solver/libsolv/helpers.hpp"

#include "progress_bar_impl.hpp"

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

        template <typename Range>
        auto make_pkg_info_from_explicit_match_specs(Range&& specs)
        {
            std::vector<specs::PackageInfo> out = {};
            out.reserve(specs.size());

            for (auto& ms : specs)
            {
                out.emplace_back(ms.name().str());
                auto& p = out.back();
                p.package_url = ms.url();
                p.build_string = ms.build_string().str();
                p.version = ms.version().str_conda_build();
                if (ms.channel().has_value())
                {
                    p.channel = ms.channel()->location();
                    if (!ms.channel()->platform_filters().empty())
                    {
                        // There must be only one since we are expecting URLs
                        assert(ms.channel()->platform_filters().size() == 1);
                        p.subdir = ms.channel()->platform_filters().front();
                    }
                }
                p.filename = ms.filename();
                p.md5 = ms.md5();
                p.sha256 = ms.sha256();
            }
            return out;
        }

        auto find_python_version(const solver::Solution& solution, const solv::ObjPool& pool)
            -> std::pair<std::string, std::string>
        {
            // We need to find the python version that will be there after this
            // Transaction is finished in order to compile the noarch packages correctly,

            // We need to look into installed packages in case we are not installing a new python
            // version but keeping the current one.
            // Could also be written in term of PrefixData.
            std::string installed_py_ver = {};
            if (auto s = solver::libsolv::installed_python(pool))
            {
                installed_py_ver = s->version();
                LOG_INFO << "Found python in installed packages " << installed_py_ver;
            }

            std::string new_py_ver = installed_py_ver;
            if (auto py = solver::find_new_python_in_solution(solution))
            {
                new_py_ver = py->get().version;
            }

            return { std::move(new_py_ver), std::move(installed_py_ver) };
        }
    }

    MTransaction::MTransaction(MPool& pool, MultiPackageCache& caches)
        : m_pool(pool)
        , m_multi_cache(caches)
        , m_history_entry(History::UserRequest::prefilled(m_pool.context()))
    {
    }

    MTransaction::MTransaction(
        MPool& pool,
        const std::vector<specs::MatchSpec>& specs_to_remove,
        const std::vector<specs::MatchSpec>& specs_to_install,
        MultiPackageCache& caches
    )
        : MTransaction(pool, caches)
    {
        auto mrepo = m_pool.add_repo_from_packages(
            make_pkg_info_from_explicit_match_specs(specs_to_install),
            "__explicit_specs__",
            solver::libsolv::PipAsPythonDependency::No
        );

        m_pool.create_whatprovides();

        // Just add the packages we want to remove directly to the transaction
        solv::ObjQueue job, decision;

        std::vector<std::string> not_found = {};
        for (auto& s : specs_to_remove)
        {
            job = {
                SOLVER_SOLVABLE_PROVIDES,
                m_pool.pool().add_conda_dependency(s.conda_build_form()),
            };

            if (const auto q = m_pool.pool().select_solvables(job); !q.empty())
            {
                for (auto& el : q)
                {
                    // To remove, these have to be negative
                    decision.push_back(-el);
                }
            }
            else
            {
                not_found.push_back("\n - " + s.str());
            }
        }

        if (!not_found.empty())
        {
            LOG_ERROR << "Could not find packages to remove:" + util::join("", not_found)
                      << std::endl;
            throw std::runtime_error("Could not find packages to remove:" + util::join("", not_found));
        }

        // TODO why is this only using the last job?
        const auto q = m_pool.pool().select_solvables(job);
        const bool remove_success = q.size() >= specs_to_remove.size();
        Console::instance().json_write({ { "success", remove_success } });

        // find repo __explicit_specs__ and install all packages from it
        auto repo = solv::ObjRepoView(*mrepo.m_ptr);
        repo.for_each_solvable_id([&](solv::SolvableId id) { decision.push_back(id); });

        auto trans = solv::ObjTransaction::from_solvables(m_pool.pool(), decision);
        // We cannot order the transaction here because we do no have dependency information
        // from the lockfile
        // TODO reload dependency information from ``ctx.target_prefix / "conda-meta"`` after
        // ``fetch_extract_packages`` is called.

        m_solution = solver::libsolv::transaction_to_solution(m_pool.pool(), trans);

        m_history_entry.remove.reserve(specs_to_remove.size());
        for (auto& s : specs_to_remove)
        {
            m_history_entry.remove.push_back(s.str());
        }
        m_history_entry.update.reserve(specs_to_install.size());
        for (auto& s : specs_to_install)
        {
            m_history_entry.update.push_back(s.str());
        }

        const auto& context = m_pool.context();

        // if no action required, don't even start logging them
        if (!empty())
        {
            Console::instance().json_down("actions");
            Console::instance().json_write({ { "PREFIX",
                                               context.prefix_params.target_prefix.string() } });
        }

        m_transaction_context = TransactionContext(
            context,
            context.prefix_params.target_prefix,
            context.prefix_params.relocate_prefix,
            find_python_version(m_solution, m_pool.pool()),
            specs_to_install
        );
    }

    MTransaction::MTransaction(
        MPool& p_pool,
        const solver::Request& request,
        solver::Solution solution,
        MultiPackageCache& caches
    )
        : MTransaction(p_pool, caches)
    {
        auto& pool = m_pool.pool();

        const auto& flags = request.flags;
        m_solution = std::move(solution);

        if (flags.keep_user_specs)
        {
            using Request = solver::Request;
            solver::for_each_of<Request::Install, Request::Update>(
                request,
                [&](const auto& item) { m_history_entry.update.push_back(item.spec.str()); }
            );
            solver::for_each_of<Request::Remove, Request::Update>(
                request,
                [&](const auto& item) { m_history_entry.remove.push_back(item.spec.str()); }
            );
        }
        else
        {
            // The specs to install become all the dependencies of the non intstalled specs
            for_each_to_omit(
                m_solution.actions,
                [&](const specs::PackageInfo& pkg)
                {
                    for (const auto& dep : pkg.depends)
                    {
                        m_history_entry.update.push_back(dep);
                    }
                }
            );
        }

        auto requested_specs = std::vector<specs::MatchSpec>();
        using Request = solver::Request;
        solver::for_each_of<Request::Install, Request::Update>(
            request,
            [&](const auto& item) { requested_specs.push_back(item.spec); }
        );
        const auto& context = m_pool.context();
        m_transaction_context = TransactionContext(
            context,
            context.prefix_params.target_prefix,
            context.prefix_params.relocate_prefix,
            find_python_version(m_solution, m_pool.pool()),
            std::move(requested_specs)
        );

        if (solver::libsolv::solution_needs_python_relink(pool, m_solution))
        {
            m_solution = solver::libsolv::add_noarch_relink_to_solution(
                std::move(m_solution),
                pool,
                "python"
            );
        }

        // if no action required, don't even start logging them
        if (!empty())
        {
            Console::instance().json_down("actions");
            Console::instance().json_write({ { "PREFIX",
                                               context.prefix_params.target_prefix.string() } });
        }
    }

    MTransaction::MTransaction(
        MPool& pool,
        const std::vector<specs::PackageInfo>& packages,
        MultiPackageCache& caches
    )
        : MTransaction(pool, caches)
    {
        LOG_INFO << "MTransaction::MTransaction - packages already resolved (lockfile)";
        auto mrepo = m_pool.add_repo_from_packages(
            packages,
            "__explicit_specs__",
            solver::libsolv::PipAsPythonDependency::No
        );
        m_pool.create_whatprovides();

        solv::ObjQueue decision = {};
        // find repo __explicit_specs__ and install all packages from it
        auto repo = solv::ObjRepoView(*mrepo.m_ptr);
        repo.for_each_solvable_id([&](solv::SolvableId id) { decision.push_back(id); });

        auto trans = solv::ObjTransaction::from_solvables(m_pool.pool(), decision);
        trans.order(m_pool.pool());

        m_solution = solver::libsolv::transaction_to_solution(m_pool.pool(), trans);

        std::vector<specs::MatchSpec> specs_to_install;
        for (const auto& pkginfo : packages)
        {
            specs_to_install.push_back(specs::MatchSpec::parse(
                fmt::format("{}=={}={}", pkginfo.name, pkginfo.version, pkginfo.build_string)
            ));
        }

        const auto& context = m_pool.context();
        m_transaction_context = TransactionContext(
            context,
            context.prefix_params.target_prefix,
            context.prefix_params.relocate_prefix,
            find_python_version(m_solution, m_pool.pool()),
            specs_to_install
        );
    }

    auto MTransaction::py_find_python_version() const -> std::pair<std::string, std::string>
    {
        return find_python_version(m_solution, m_pool.pool());
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

    bool MTransaction::execute(PrefixData& prefix)
    {
        using Solution = solver::Solution;

        auto& ctx = m_pool.context();

        // JSON output
        // back to the top level if any action was required
        if (!empty())
        {
            Console::instance().json_up();
        }
        Console::instance().json_write({ { "dry_run", ctx.dry_run },
                                         { "prefix", ctx.prefix_params.target_prefix.string() } });
        if (empty())
        {
            Console::instance().json_write({ { "message",
                                               "All requested packages already installed" } });
        }

        if (ctx.dry_run)
        {
            Console::stream() << "Dry run. Not executing the transaction.";
            return true;
        }

        auto lf = LockFile(ctx.prefix_params.target_prefix / "conda-meta");
        clean_trash_files(ctx.prefix_params.target_prefix, false);

        Console::stream() << "\nTransaction starting";
        fetch_extract_packages();

        if (ctx.download_only)
        {
            Console::stream(
            ) << "Download only - packages are downloaded and extracted. Skipping the linking phase.";
            return true;
        }

        TransactionRollback rollback;

        const auto execute_action = [&](const auto& act)
        {
            using Action = std::decay_t<decltype(act)>;

            auto const link = [&](specs::PackageInfo const& pkg)
            {
                const fs::u8path cache_path(m_multi_cache.get_extracted_dir_path(pkg, false));
                LinkPackage lp(pkg, cache_path, &m_transaction_context);
                lp.execute();
                rollback.record(lp);
                m_history_entry.link_dists.push_back(pkg.long_str());
            };
            auto const unlink = [&](specs::PackageInfo const& pkg)
            {
                const fs::u8path cache_path(m_multi_cache.get_extracted_dir_path(pkg));
                UnlinkPackage up(pkg, cache_path, &m_transaction_context);
                up.execute();
                rollback.record(up);
                m_history_entry.unlink_dists.push_back(pkg.long_str());
            };

            if constexpr (std::is_same_v<Action, Solution::Reinstall>)
            {
                Console::stream() << "Reinstalling " << act.what.str();
                unlink(act.what);
                link(act.what);
            }
            else if constexpr (Solution::has_remove_v<Action> && Solution::has_install_v<Action>)
            {
                Console::stream() << "Changing " << act.remove.str() << " ==> " << act.install.str();
                unlink(act.remove);
                link(act.install);
            }
            else if constexpr (Solution::has_remove_v<Action>)
            {
                Console::stream() << "Unlinking " << act.remove.str();
                unlink(act.remove);
            }
            else if constexpr (Solution::has_install_v<Action>)
            {
                Console::stream() << "Linking " << act.install.str();
                link(act.install);
            }
        };

        for (const auto& action : m_solution.actions)
        {
            if (is_sig_interrupted())
            {
                break;
            }
            std::visit(execute_action, action);
        }

        if (is_sig_interrupted())
        {
            Console::stream() << "Transaction interrupted, rollbacking";
            rollback.rollback(ctx);
            return false;
        }
        LOG_INFO << "Waiting for pyc compilation to finish";
        m_transaction_context.wait_for_pyc_compilation();

        // Get the name of the executable used directly from the command.
        const auto executable = ctx.command_params.is_micromamba ? "micromamba" : "mamba";

        // Get the name of the environment
        const auto environment = env_name(ctx);

        Console::stream() << "\nTransaction finished\n\n"
                             "To activate this environment, use:\n\n"
                             "    "
                          << executable << " activate " << environment
                          << "\n\n"
                             "Or to execute a single command in this environment, use:\n\n"
                             "    "
                          << executable
                          << " run "
                          // Use -n or -p depending on if the env_name is a full prefix or just
                          // a name.
                          << (environment == ctx.prefix_params.target_prefix ? "-p " : "-n ")
                          << environment << " mycommand\n";

        prefix.history().add_entry(m_history_entry);
        return true;
    }

    auto MTransaction::to_conda() -> to_conda_type
    {
        to_remove_type to_remove_structured = {};
        to_remove_structured.reserve(m_solution.actions.size());  // Upper bound
        for_each_to_remove(
            m_solution.actions,
            [&](const auto& pkg)
            {
                to_remove_structured.emplace_back(pkg.channel, pkg.filename);  //
            }
        );

        to_install_type to_install_structured = {};
        to_install_structured.reserve(m_solution.actions.size());  // Upper bound
        for_each_to_install(
            m_solution.actions,
            [&](const auto& pkg)
            {
                to_install_structured.emplace_back(pkg.channel, pkg.filename, nl::json(pkg).dump(4));  //
            }
        );

        to_specs_type specs;
        std::get<0>(specs) = m_history_entry.update;
        std::get<1>(specs) = m_history_entry.remove;

        return std::make_tuple(specs, to_install_structured, to_remove_structured);
    }

    void MTransaction::log_json()
    {
        std::vector<nl::json> to_fetch, to_link, to_unlink;

        for_each_to_install(
            m_solution.actions,
            [&](const auto& pkg)
            {
                if (need_pkg_download(pkg, m_multi_cache))
                {
                    to_fetch.push_back(nl::json(pkg));
                }
                to_link.push_back(nl::json(pkg));
            }
        );

        for_each_to_remove(
            m_solution.actions,
            [&](const auto& pkg)
            {
                to_unlink.push_back(nl::json(pkg));  //
            }
        );

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
        FetcherList
        build_fetchers(MPool& pool, const solver::Solution& solution, MultiPackageCache& multi_cache)
        {
            FetcherList fetchers;
            auto& channel_context = pool.channel_context();
            auto& ctx = pool.context();

            if (ctx.experimental && ctx.validation_params.verify_artifacts)
            {
                LOG_INFO << "Content trust is enabled, package(s) signatures will be verified";
            }
            for_each_to_install(
                solution.actions,
                [&](const auto& pkg)
                {
                    // The following was used for the The Update Framework (TUF) / package signing
                    // proof of concept.
                    //
                    // Due to uncertainties on how TUF would be implemented, this was left commented
                    // out as in was getting in the way of the Channel refactoring.

                    // In channel.cpp, repo-checkers were instanciated with the folowing:
                    // const validation::RepoChecker&
                    //    Channel::repo_checker(Context& context, MultiPackageCache& caches) const
                    //    {
                    //        if (p_repo_checker == nullptr)
                    //        {
                    //            p_repo_checker = std::make_unique<validation::RepoChecker>(
                    //                context,
                    //                util::rsplit(base_url(), "/", 1).front(),
                    //                context.prefix_params.root_prefix / "etc" / "trusted-repos"
                    //                    / util::cache_name_from_url(base_url()),
                    //                caches.first_writable_path() / "cache" /
                    //                util::cache_name_from_url(base_url())
                    //            );
                    //
                    //            fs::create_directories(p_repo_checker->cache_path());
                    //            p_repo_checker->generate_index_checker();
                    //        }
                    //
                    //        return *p_repo_checker;
                    //    }

                    // Here, the repo-checker would be fetched the following way:
                    // if (ctx.experimental && ctx.validation_params.verify_artifacts)
                    // {
                    //     const auto& repo_checker = channel_context.make_channel(pkg.channel)
                    //                                    .repo_checker(ctx, multi_cache);
                    //     repo_checker.verify_package(
                    //         pkg.json_signable(),
                    //         nlohmann::json::parse(pkg.signatures)
                    //     );
                    //
                    //     LOG_DEBUG << "'" << pkg.name << "' trusted from '" << pkg.channel << "'";
                    // }

                    // FIXME: only do this for micromamba for now
                    if (ctx.command_params.is_micromamba)
                    {
                        using Credentials = typename specs::CondaURL::Credentials;
                        auto l_pkg = pkg;
                        auto channels = channel_context.make_channel(pkg.package_url);
                        assert(channels.size() == 1);  // A URL can only resolve to one channel
                        l_pkg.package_url = channels.front().platform_urls().at(0).str(Credentials::Show
                        );
                        fetchers.emplace_back(l_pkg, multi_cache);
                    }
                    else
                    {
                        fetchers.emplace_back(pkg, multi_cache);
                    }
                }
            );

            if (ctx.experimental && ctx.validation_params.verify_artifacts)
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
                std::packaged_task task{ [=] { return it->run(); } };
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
            auto result = download::download(std::move(requests), context.mirrors, context, options, monitor);
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

    bool MTransaction::fetch_extract_packages()
    {
        auto& ctx = m_pool.context();
        PackageFetcherSemaphore::set_max(ctx.threads_params.extract_threads);

        FetcherList fetchers = build_fetchers(m_pool, m_solution, m_multi_cache);

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
        download::Options download_options{ true, true };
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

    bool MTransaction::prompt()
    {
        print();
        if (m_pool.context().dry_run || empty())
        {
            return true;
        }

        return Console::prompt("Confirm changes", 'y');
    }

    void MTransaction::print()
    {
        using Solution = solver::Solution;

        const auto& ctx = m_pool.context();

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
                Console::instance().print("  Failure: packages to remove not found in the environment:\n"
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
        t.set_alignment({ printers::alignment::left,
                          printers::alignment::right,
                          printers::alignment::left,
                          printers::alignment::left,
                          printers::alignment::right });
        t.set_padding({ 2, 2, 2, 2, 5 });
        solv::ObjQueue classes = {};
        solv::ObjQueue pkgs = {};

        using rows = std::vector<std::vector<printers::FormattedString>>;

        rows downgraded, upgraded, changed, reinstalled, erased, installed, ignored;
        std::size_t total_size = 0;

        enum struct Status
        {
            install,
            ignore,
            remove
        };
        auto format_row =
            [this,
             &ctx,
             &total_size](rows& r, const specs::PackageInfo& s, Status status, std::string diff)
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
                    auto channels = m_pool.channel_context().make_channel(str);
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

            r.push_back({ name,
                          printers::FormattedString(s.version),
                          printers::FormattedString(s.build_string),
                          printers::FormattedString(cut_repo_name(chan_name)),
                          dlsize_s });
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

    MTransaction
    create_explicit_transaction_from_urls(MPool& pool, const std::vector<std::string>& urls, MultiPackageCache& package_caches, std::vector<detail::other_pkg_mgr_spec>&)
    {
        std::vector<specs::MatchSpec> specs_to_install = {};
        specs_to_install.reserve(urls.size());
        for (auto& raw_url : urls)
        {
            std::string_view url = util::strip(raw_url);
            if (url.empty())
            {
                continue;
            }

            const auto hash_idx = url.find_first_of('#');
            specs_to_install.emplace_back(specs::MatchSpec::parse(url.substr(0, hash_idx)));
            specs::MatchSpec& ms = specs_to_install.back();

            if (hash_idx != std::string::npos)
            {
                std::string_view hash = url.substr(hash_idx + 1);
                if (util::starts_with(hash, "sha256:"))
                {
                    ms.set_sha256(std::string(hash.substr(7)));
                }
                if (util::starts_with(hash, "md5:"))
                {
                    ms.set_sha256(std::string(hash.substr(4)));
                }
                else
                {
                    ms.set_md5(std::string(hash));
                }
            }
        }
        return MTransaction(pool, {}, specs_to_install, package_caches);
    }

    MTransaction create_explicit_transaction_from_lockfile(
        MPool& pool,
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

        std::vector<specs::PackageInfo> conda_packages = {};
        std::vector<specs::PackageInfo> pip_packages = {};

        const auto& context = pool.context();
        for (const auto& category : categories)
        {
            std::vector<specs::PackageInfo> selected_packages = lockfile_data.get_packages_for(
                category,
                context.platform,
                "conda"
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
                            << context.platform << ").";
            }

            selected_packages = lockfile_data.get_packages_for(category, context.platform, "pip");
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

        return MTransaction{ pool, conda_packages, package_caches };
    }

}  // namespace mamba
