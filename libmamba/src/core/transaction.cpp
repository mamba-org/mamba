// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <solv/selection.h>
extern "C"  // Incomplete header
{
#include <solv/conda.h>
}

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/env_lockfile.hpp"
#include "mamba/core/link.hpp"
#include "mamba/core/match_spec.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_download.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/util_string.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/queue.hpp"
#include "solv-cpp/repo.hpp"
#include "solv-cpp/solver.hpp"
#include "solv-cpp/transaction.hpp"

#include "progress_bar_impl.hpp"

namespace mamba
{
    namespace
    {
        bool need_pkg_download(const PackageInfo& pkg_info, MultiPackageCache& caches)
        {
            return caches.get_extracted_dir_path(pkg_info).empty()
                   && caches.get_tarball_path(pkg_info).empty();
        }

        auto mk_pkginfo(const MPool& pool, solv::ObjSolvableViewConst s) -> PackageInfo
        {
            const auto pkginfo = pool.id2pkginfo(s.id());
            assert(pkginfo.has_value());  // There is Solvable so the optional must no be empty
            return std::move(pkginfo).value();
        };

        nlohmann::json solvable_to_json(const MPool& pool, solv::ObjSolvableViewConst s)
        {
            return mk_pkginfo(pool, s).json_record();
        }

        template <typename Range>
        auto make_pkg_info_from_explicit_match_specs(Range&& specs)
        {
            std::vector<PackageInfo> out = {};
            out.reserve(specs.size());

            for (auto& ms : specs)
            {
                out.emplace_back(ms.name);
                auto& p = out.back();
                p.url = ms.url;
                p.build_string = ms.build_string;
                p.version = ms.version;
                p.channel = ms.channel;
                p.fn = ms.fn;
                p.subdir = ms.subdir;
                if (ms.brackets.find("md5") != ms.brackets.end())
                {
                    p.md5 = ms.brackets.at("md5");
                }
                if (ms.brackets.find("sha256") != ms.brackets.end())
                {
                    p.sha256 = ms.brackets.at("sha256");
                }
            }
            return out;
        }
    }

    MTransaction::MTransaction(
        MPool& pool,
        const std::vector<MatchSpec>& specs_to_remove,
        const std::vector<MatchSpec>& specs_to_install,
        MultiPackageCache& caches
    )
        : m_pool(pool)
        , m_multi_cache(caches)
    {
        MRepo mrepo = MRepo(
            m_pool,
            "__explicit_specs__",
            make_pkg_info_from_explicit_match_specs(specs_to_install)
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
            LOG_ERROR << "Could not find packages to remove:" + join("", not_found) << std::endl;
            throw std::runtime_error("Could not find packages to remove:" + join("", not_found));
        }

        // TODO why is this only using the last job?
        const auto q = m_pool.pool().select_solvables(job);
        const bool remove_success = q.size() >= specs_to_remove.size();
        Console::instance().json_write({{"success", remove_success}});

        // find repo __explicit_specs__ and install all packages from it
        auto repo = solv::ObjRepoView(*mrepo.repo());
        repo.for_each_solvable_id([&](solv::SolvableId id) { decision.push_back(id); });

        m_transaction = std::make_unique<solv::ObjTransaction>(
            solv::ObjTransaction::from_solvables(m_pool.pool(), decision)
        );
        // We cannot order the transcation here because we do no have dependency information
        // from the lockfile
        // TODO reload dependency information from ``ctx.target_prefix / "conda-meta"`` after
        // ``fetch_extract_packages`` is called.
        init();

        for (auto& s : specs_to_remove)
        {
            m_history_entry.remove.push_back(s.str());
        }
        for (auto& s : specs_to_install)
        {
            m_history_entry.update.push_back(s.str());
        }

        // if no action required, don't even start logging them
        if (!empty())
        {
            Console::instance().json_down("actions");
            Console::instance().json_write(
                {{"PREFIX", Context::instance().prefix_params.target_prefix.string()}}
            );
        }

        m_transaction_context = TransactionContext(
            Context::instance().prefix_params.target_prefix,
            Context::instance().prefix_params.relocate_prefix,
            find_python_version(),
            specs_to_install
        );
    }

    MTransaction::MTransaction(MPool& p_pool, MSolver& solver, MultiPackageCache& caches)
        : m_pool(p_pool)
        , m_multi_cache(caches)
    {
        if (!solver.is_solved())
        {
            throw std::runtime_error("Cannot create transaction without calling solver.solve() first."
            );
        }
        auto& pool = m_pool.pool();

        m_transaction = std::make_unique<solv::ObjTransaction>(
            solv::ObjTransaction::from_solver(pool, solver.solver())
        );
        trans().order(pool);

        if (solver.no_deps || solver.only_deps)
        {
            m_filter_type = solver.only_deps ? FilterType::keep_only : FilterType::ignore;
            for (auto& s : solver.install_specs())
            {
                m_filter_name_ids.insert(pool.add_string(s.name));
            }
            for (auto& s : solver.remove_specs())
            {
                m_filter_name_ids.insert(pool.add_string(s.name));
            }
        }

        if (solver.only_deps)
        {
            for (const solv::SolvableId r : trans().steps())
            {
                auto s = pool.get_solvable(r);
                assert(s.has_value());
                if (m_filter_name_ids.count(s->raw()->name))
                {
                    for (const auto dep_id : s->dependencies())
                    {
                        std::string add_spec = std::string(pool.get_dependency_name(dep_id));
                        if (auto version = pool.get_dependency_version(dep_id); !version.empty())
                        {
                            add_spec += ' ';
                            add_spec += version;
                        }
                        m_history_entry.update.push_back(
                            MatchSpec{add_spec, m_pool.channel_context()}.str()
                        );
                    }
                }
            }
        }
        else
        {
            auto to_string_vec = [](const std::vector<MatchSpec>& vec) -> std::vector<std::string>
            {
                std::vector<std::string> res = {};
                res.reserve(vec.size());
                std::transform(
                    vec.cbegin(),
                    vec.cend(),
                    std::back_inserter(res),
                    [](auto const& el) { return el.str(); }
                );
                return res;
            };
            m_history_entry.update = to_string_vec(solver.install_specs());
            m_history_entry.remove = to_string_vec(solver.remove_specs());
        }

        m_force_reinstall = solver.force_reinstall;

        init();
        // if no action required, don't even start logging them
        if (!empty())
        {
            Console::instance().json_down("actions");
            Console::instance().json_write(
                {{"PREFIX", Context::instance().prefix_params.target_prefix.string()}}
            );
        }

        m_transaction_context = TransactionContext(
            Context::instance().prefix_params.target_prefix,
            Context::instance().prefix_params.relocate_prefix,
            find_python_version(),
            solver.install_specs()
        );

        if (auto maybe_installed = pool.installed_repo();
            m_transaction_context.relink_noarch && maybe_installed.has_value())
        {
            // TODO could we use the transaction instead?
            solv::ObjQueue decision = {};
            solver_get_decisionqueue(solver.solver().raw(), decision.raw());

            pool.for_each_installed_solvable(
                [&](solv::ObjSolvableViewConst s)
                {
                    if (s.noarch() == "python")
                    {
                        auto id = s.id();
                        auto id_iter = std::find_if(
                            decision.cbegin(),
                            decision.cend(),
                            [id](auto other) { return std::abs(other) == id; }
                        );

                        // if the installed package is kept, we should relink
                        if ((id_iter != decision.cend()) && (*id_iter == id))
                        {
                            // Remove old linked package
                            decision.erase(id_iter);

                            const auto pkg_info = mk_pkginfo(m_pool, s);
                            solv::ObjQueue const job = {
                                SOLVER_SOLVABLE_PROVIDES,
                                pool.add_conda_dependency(fmt::format(
                                    "{} {} {}",
                                    pkg_info.name,
                                    pkg_info.version,
                                    pkg_info.build_string
                                )),
                            };

                            const auto matches = pool.select_solvables(job);
                            const auto reinstall_iter = std::find_if(
                                matches.cbegin(),
                                matches.cend(),
                                [&](solv::SolvableId r)
                                {
                                    auto rsolv = pool.get_solvable(r);
                                    return rsolv.has_value() && !rsolv->installed();
                                }
                            );
                            if (reinstall_iter == matches.cend())
                            {
                                // TODO we should also search the local package cache to make
                                // offline installs work
                                LOG_WARNING << fmt::format(
                                    "To upgrade python we need to reinstall noarch",
                                    " package {} {} {} but we could not find it in",
                                    " any of the loaded channels.",
                                    pkg_info.name,
                                    pkg_info.version,
                                    pkg_info.build_string
                                );
                            }
                            else
                            {
                                decision.push_back(*reinstall_iter);
                                decision.push_back(-id);
                            }
                        }
                    }
                }
            );

            m_transaction = std::make_unique<solv::ObjTransaction>(
                solv::ObjTransaction::from_solvables(m_pool.pool(), decision)
            );
            trans().order(m_pool.pool());

            // init everything again...
            init();
        }
    }

    MTransaction::MTransaction(
        MPool& pool,
        const std::vector<PackageInfo>& packages,
        MultiPackageCache& caches
    )
        : m_pool(pool)
        , m_multi_cache(caches)
    {
        LOG_INFO << "MTransaction::MTransaction - packages already resolved (lockfile)";
        MRepo mrepo = MRepo(m_pool, "__explicit_specs__", packages);
        m_pool.create_whatprovides();

        solv::ObjQueue decision = {};
        // find repo __explicit_specs__ and install all packages from it
        auto repo = solv::ObjRepoView(*mrepo.repo());
        repo.for_each_solvable_id([&](solv::SolvableId id) { decision.push_back(id); });

        m_transaction = std::make_unique<solv::ObjTransaction>(
            solv::ObjTransaction::from_solvables(m_pool.pool(), decision)
        );
        trans().order(m_pool.pool());

        init();

        std::vector<MatchSpec> specs_to_install;
        for (const auto& pkginfo : packages)
        {
            specs_to_install.push_back(MatchSpec(
                fmt::format("{}=={}={}", pkginfo.name, pkginfo.version, pkginfo.build_string),
                m_pool.channel_context()
            ));
        }

        m_transaction_context = TransactionContext(
            Context::instance().prefix_params.target_prefix,
            Context::instance().prefix_params.relocate_prefix,
            find_python_version(),
            specs_to_install
        );
    }

    MTransaction::~MTransaction() = default;

    auto MTransaction::trans() -> solv::ObjTransaction&
    {
        assert(m_transaction != nullptr);
        return *m_transaction;
    }

    auto MTransaction::trans() const -> const solv::ObjTransaction&
    {
        assert(m_transaction != nullptr);
        return *m_transaction;
    }

    void MTransaction::init()
    {
        m_to_remove.clear();
        m_to_install.clear();
        const auto& pool = m_pool.pool();
        trans().for_each_step_solvable(
            pool,
            [&](auto s)
            {
                assert(s.has_value());
                if (!filter(*s))
                {
                    auto const type = trans().step_type(pool, s->id(), SOLVER_TRANSACTION_SHOW_ALL);
                    switch (type)
                    {
                        case SOLVER_TRANSACTION_DOWNGRADED:
                        case SOLVER_TRANSACTION_UPGRADED:
                        case SOLVER_TRANSACTION_CHANGED:
                        case SOLVER_TRANSACTION_REINSTALLED:
                        {
                            m_to_remove.emplace_back(*s);
                            // Packages that replace these one will show up under IGNORE
                            // so we need to fetch them here
                            if (auto maybe_newer = trans().step_newer(pool, s->id()))
                            {
                                auto solvable = pool.get_solvable(*maybe_newer);
                                assert(solvable);
                                m_to_install.push_back(*solvable);
                            }
                            break;
                        }
                        case SOLVER_TRANSACTION_ERASE:
                        {
                            m_to_remove.emplace_back(*s);
                            break;
                        }
                        case SOLVER_TRANSACTION_INSTALL:
                        {
                            m_to_install.emplace_back(*s);
                            break;
                        }
                        case SOLVER_TRANSACTION_IGNORE:
                            break;
                        default:
                            LOG_ERROR << "Exec case not handled: " << type;
                            break;
                    }
                }
            }
        );
    }

    bool MTransaction::filter(const solv::ObjSolvableViewConst& s)
    {
        if (m_filter_type == FilterType::none)
        {
            return false;
        }
        bool spec_in_filter = m_filter_name_ids.count(s.raw()->name);

        if (m_filter_type == FilterType::keep_only)
        {
            return spec_in_filter;
        }
        else
        {
            return !spec_in_filter;
        }
    }

    // TODO rewrite this in terms of `m_transaction`
    std::pair<std::string, std::string> MTransaction::find_python_version()
    {
        // We need to find the python version that will be there after this
        // Transaction is finished in order to compile the noarch packages correctly,
        // for example

        std::string installed_py_ver = {};
        std::string new_py_ver = {};

        for (auto s : m_to_install)
        {
            if (s.name() == "python")
            {
                new_py_ver = s.version();
                LOG_INFO << "Found python version in packages to be installed " << new_py_ver;
                break;
            }
        }

        m_pool.pool().for_each_installed_solvable(
            [&](solv::ObjSolvableViewConst s)
            {
                if (s.name() == "python")
                {
                    installed_py_ver = s.version();
                    LOG_INFO << "Found python in installed packages " << installed_py_ver;
                }
            }
        );

        // if we do not install a new python version but keep the current one
        if (new_py_ver.empty())
        {
            new_py_ver = installed_py_ver;
        }
        return std::make_pair(new_py_ver, installed_py_ver);
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

        void rollback()
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
        auto& ctx = Context::instance();

        // JSON output
        // back to the top level if any action was required
        if (!empty())
        {
            Console::instance().json_up();
        }
        Console::instance().json_write(
            {{"dry_run", ctx.dry_run}, {"prefix", ctx.prefix_params.target_prefix.string()}}
        );
        if (empty())
        {
            Console::instance().json_write({{"message", "All requested packages already installed"}});
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

        const auto& pool = m_pool.pool();
        for (solv::SolvableId const p : trans().steps())
        {
            if (is_sig_interrupted())
            {
                break;
            }
            auto s = pool.get_solvable(p);
            assert(s.has_value());
            if (filter(*s))
            {
                continue;
            }

            const auto ttype = trans().step_type(pool, p, SOLVER_TRANSACTION_SHOW_ALL);
            switch (ttype)
            {
                case SOLVER_TRANSACTION_DOWNGRADED:
                case SOLVER_TRANSACTION_UPGRADED:
                case SOLVER_TRANSACTION_CHANGED:
                case SOLVER_TRANSACTION_REINSTALLED:
                {
                    auto newer = [&]()
                    {
                        auto maybe_newer_id = trans().step_newer(pool, s->id());
                        assert(maybe_newer_id.has_value());
                        auto maybe_newer = pool.get_solvable(maybe_newer_id.value());
                        assert(maybe_newer.has_value());
                        return maybe_newer.value();
                    }();

                    const PackageInfo package_to_unlink = mk_pkginfo(m_pool, *s);
                    const PackageInfo package_to_link = mk_pkginfo(m_pool, newer);

                    Console::stream() << "Changing " << package_to_unlink.str() << " ==> "
                                      << package_to_link.str();

                    const fs::u8path ul_cache_path(
                        m_multi_cache.get_extracted_dir_path(package_to_unlink)
                    );

                    const fs::u8path l_cache_path(
                        m_multi_cache.get_extracted_dir_path(package_to_link, false)
                    );

                    UnlinkPackage up(package_to_unlink, ul_cache_path, &m_transaction_context);
                    up.execute();
                    rollback.record(up);

                    LinkPackage lp(package_to_link, l_cache_path, &m_transaction_context);
                    lp.execute();
                    rollback.record(lp);

                    m_history_entry.unlink_dists.push_back(package_to_unlink.long_str());
                    m_history_entry.link_dists.push_back(package_to_link.long_str());

                    break;
                }
                case SOLVER_TRANSACTION_ERASE:
                {
                    PackageInfo package_info = mk_pkginfo(m_pool, *s);
                    Console::stream() << "Unlinking " << package_info.str();
                    const fs::u8path cache_path(m_multi_cache.get_extracted_dir_path(package_info));
                    UnlinkPackage up(package_info, cache_path, &m_transaction_context);
                    up.execute();
                    rollback.record(up);
                    m_history_entry.unlink_dists.push_back(package_info.long_str());
                    break;
                }
                case SOLVER_TRANSACTION_INSTALL:
                {
                    PackageInfo package_info = mk_pkginfo(m_pool, *s);
                    Console::stream() << "Linking " << package_info.str();
                    const fs::u8path cache_path(
                        m_multi_cache.get_extracted_dir_path(package_info, false)
                    );
                    LinkPackage lp(package_info, cache_path, &m_transaction_context);
                    lp.execute();
                    rollback.record(lp);
                    m_history_entry.link_dists.push_back(package_info.long_str());
                    break;
                }
                case SOLVER_TRANSACTION_IGNORE:
                    break;
                default:
                    LOG_ERROR << "Exec case not handled: " << ttype;
                    break;
            }
        }

        if (is_sig_interrupted())
        {
            Console::stream() << "Transaction interrupted, rollbacking";
            rollback.rollback();
            return false;
        }
        LOG_INFO << "Waiting for pyc compilation to finish";
        m_transaction_context.wait_for_pyc_compilation();

        // Get the name of the executable used directly from the command.
        const auto executable = ctx.command_params.is_micromamba ? "micromamba" : "mamba";

        // Get the name of the environment
        const auto environment = env_name(ctx.prefix_params.target_prefix);

        Console::stream() << "\nTransaction finished\n\n"
                          << "To activate this environment, use:\n\n"
                          << "    " << executable << " activate " << environment << "\n\n"
                          << "Or to execute a single command in this environment, use:\n\n"
                          << "    " << executable
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
        to_install_type to_install_structured;
        to_remove_type to_remove_structured;

        for (auto s : m_to_remove)
        {
            to_remove_structured.emplace_back(
                solv::ObjRepoViewConst::of_solvable(s).name(),
                s.file_name()
            );
        }

        for (auto s : m_to_install)
        {
            std::string s_json = solvable_to_json(m_pool, s).dump(4);

            std::string chan_name;
            if (auto str = s.channel(); !str.empty())
            {
                chan_name = str;
            }
            else
            {
                // note this can and should be <unknown> when e.g. installing from a tarball
                chan_name = solv::ObjRepoViewConst::of_solvable(s).name();
            }

            to_install_structured.emplace_back(chan_name, s.file_name(), s_json);
        }

        to_specs_type specs;
        std::get<0>(specs) = m_history_entry.update;
        std::get<1>(specs) = m_history_entry.remove;

        return std::make_tuple(specs, to_install_structured, to_remove_structured);
    }

    void MTransaction::log_json()
    {
        std::vector<nlohmann::json> to_fetch, to_link, to_unlink;

        for (auto s : m_to_install)
        {
            if (!need_pkg_download(mk_pkginfo(m_pool, s), m_multi_cache))
            {
                to_link.push_back(solvable_to_json(m_pool, s));
            }
            else
            {
                to_fetch.push_back(solvable_to_json(m_pool, s));
                to_link.push_back(solvable_to_json(m_pool, s));
            }
        }

        for (auto s : m_to_remove)
        {
            to_unlink.push_back(solvable_to_json(m_pool, s));
        }

        auto add_json = [](const auto& jlist, const char* s)
        {
            if (!jlist.empty())
            {
                Console::instance().json_down(s);
                for (nlohmann::json j : jlist)
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

    bool MTransaction::fetch_extract_packages()
    {
        std::vector<std::unique_ptr<PackageDownloadExtractTarget>> targets;
        MultiDownloadTarget multi_dl;

        auto& pbar_manager = Console::instance().init_progress_bar_manager(ProgressBarMode::aggregated
        );
        auto& aggregated_pbar_manager = dynamic_cast<AggregatedBarManager&>(pbar_manager);

        auto& ctx = Context::instance();
        DownloadExtractSemaphore::set_max(ctx.threads_params.extract_threads);

        if (ctx.experimental && ctx.verify_artifacts)
        {
            LOG_INFO << "Content trust is enabled, package(s) signatures will be verified";
        }

        for (auto& s : m_to_install)
        {
            const auto s_url = solv::ObjRepoViewConst::of_solvable(s).url();

            if (ctx.experimental && ctx.verify_artifacts)
            {
                const Channel& chan = m_pool.channel_context().make_channel(std::string(s_url));
                const auto& repo_checker = chan.repo_checker(m_multi_cache);
                const auto pkg_info = mk_pkginfo(m_pool, s);
                repo_checker.verify_package(
                    pkg_info.json_signable(),
                    nlohmann::json::parse(pkg_info.signatures)
                );

                LOG_DEBUG << "'" << pkg_info.name << "' trusted from '" << s_url << "'";
            }

            targets.emplace_back(std::make_unique<PackageDownloadExtractTarget>(
                mk_pkginfo(m_pool, s),
                m_pool.channel_context()
            ));
            DownloadTarget* download_target = targets.back()->target(m_multi_cache);
            if (download_target != nullptr)
            {
                multi_dl.add(download_target);
            }
        }

        if (ctx.experimental && ctx.verify_artifacts)
        {
            auto out = Console::stream();
            fmt::print(
                out,
                "Content trust verifications successful, {} ",
                fmt::styled("package(s) are trusted", Context::instance().graphics_params.palette.safe)
            );
            LOG_INFO << "All package(s) are trusted";
        }

        if (!(ctx.graphics_params.no_progress_bars || ctx.output_params.json
              || ctx.output_params.quiet))
        {
            interruption_guard g([]() { Console::instance().progress_bar_manager().terminate(); });

            auto* dl_bar = aggregated_pbar_manager.aggregated_bar("Download");
            if (dl_bar)
            {
                dl_bar->set_repr_hook(
                    [=](ProgressBarRepr& repr) -> void
                    {
                        auto active_tasks = dl_bar->active_tasks().size();
                        if (active_tasks == 0)
                        {
                            repr.prefix.set_value(fmt::format("{:<16}", "Downloading"));
                            repr.postfix.set_value(fmt::format("{:<25}", ""));
                        }
                        else
                        {
                            repr.prefix.set_value(fmt::format(
                                "{:<11} {:>4}",
                                "Downloading",
                                fmt::format("({})", active_tasks)
                            ));
                            repr.postfix.set_value(fmt::format("{:<25}", dl_bar->last_active_task()));
                        }
                        repr.current.set_value(fmt::format(
                            "{:>7}",
                            to_human_readable_filesize(double(dl_bar->current()), 1)
                        ));
                        repr.separator.set_value("/");

                        std::string total_str;
                        if (dl_bar->total() == std::numeric_limits<std::size_t>::max())
                        {
                            total_str = "??.?MB";
                        }
                        else
                        {
                            total_str = to_human_readable_filesize(double(dl_bar->total()), 1);
                        }
                        repr.total.set_value(fmt::format("{:>7}", total_str));

                        auto speed = dl_bar->avg_speed(std::chrono::milliseconds(500));
                        repr.speed.set_value(
                            speed
                                ? fmt::format("@ {:>7}/s", to_human_readable_filesize(double(speed), 1))
                                : ""
                        );
                    }
                );
            }

            auto* extract_bar = aggregated_pbar_manager.aggregated_bar("Extract");
            if (extract_bar)
            {
                extract_bar->set_repr_hook(
                    [=](ProgressBarRepr& repr) -> void
                    {
                        auto active_tasks = extract_bar->active_tasks().size();
                        if (active_tasks == 0)
                        {
                            repr.prefix.set_value(fmt::format("{:<16}", "Extracting"));
                            repr.postfix.set_value(fmt::format("{:<25}", ""));
                        }
                        else
                        {
                            repr.prefix.set_value(fmt::format(
                                "{:<11} {:>4}",
                                "Extracting",
                                fmt::format("({})", active_tasks)
                            ));
                            repr.postfix.set_value(
                                fmt::format("{:<25}", extract_bar->last_active_task())
                            );
                        }
                        repr.current.set_value(fmt::format("{:>3}", extract_bar->current()));
                        repr.separator.set_value("/");

                        std::string total_str;
                        if (extract_bar->total() == std::numeric_limits<std::size_t>::max())
                        {
                            total_str = "?";
                        }
                        else
                        {
                            total_str = std::to_string(extract_bar->total());
                        }
                        repr.total.set_value(fmt::format("{:>3}", total_str));
                    }
                );
            }

            pbar_manager.start();
            pbar_manager.watch_print();
        }

        bool downloaded = multi_dl.download(MAMBA_DOWNLOAD_FAILFAST | MAMBA_DOWNLOAD_SORT);
        bool all_valid = true;

        if (!downloaded)
        {
            LOG_ERROR << "Download didn't finish!";
            return false;
        }
        // make sure that all targets have finished extracting
        while (!is_sig_interrupted())
        {
            bool all_finished = true;
            for (const auto& t : targets)
            {
                if (!t->finished())
                {
                    all_finished = false;
                    break;
                }
            }
            if (all_finished)
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!(ctx.graphics_params.no_progress_bars || ctx.output_params.json
              || ctx.output_params.quiet))
        {
            pbar_manager.terminate();
            pbar_manager.clear_progress_bars();
        }

        for (const auto& t : targets)
        {
            if (t->validation_result() != PackageDownloadExtractTarget::VALIDATION_RESULT::VALID
                && t->validation_result()
                       != PackageDownloadExtractTarget::VALIDATION_RESULT::UNDEFINED)
            {
                t->clear_cache();
                all_valid = false;
                throw std::runtime_error(
                    std::string("Found incorrect download: ") + t->name() + ". Aborting"
                );
            }
        }

        return !is_sig_interrupted() && downloaded && all_valid;
    }

    bool MTransaction::empty()
    {
        return m_to_install.size() == 0 && m_to_remove.size() == 0;
    }

    bool MTransaction::prompt()
    {
        print();
        if (Context::instance().dry_run || empty())
        {
            return true;
        }

        return Console::prompt("Confirm changes", 'y');
    }

    void MTransaction::print()
    {
        const auto& ctx = Context::instance();

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

        printers::Table t({"Package", "Version", "Build", "Channel", "Size"});
        t.set_alignment(
            {printers::alignment::left,
             printers::alignment::right,
             printers::alignment::left,
             printers::alignment::left,
             printers::alignment::right}
        );
        t.set_padding({2, 2, 2, 2, 5});
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
             &total_size](rows& r, solv::ObjSolvableViewConst s, Status status, std::string diff)
        {
            std::size_t const dlsize = s.size();
            printers::FormattedString dlsize_s;
            if (dlsize > 0)
            {
                if (status == Status::ignore)
                {
                    dlsize_s.s = "Ignored";
                }
                else
                {
                    if (!need_pkg_download(mk_pkginfo(m_pool, s), m_multi_cache))
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
            name.s = fmt::format("{} {}", diff, s.name());
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
            if (auto str = s.channel(); !str.empty())
            {
                if (str == "explicit_specs")
                {
                    chan_name = s.file_name();
                }
                else
                {
                    const Channel& chan = m_pool.channel_context().make_channel(std::string(str));
                    chan_name = chan.canonical_name();
                }
            }
            else
            {
                // note this can and should be <unknown> when e.g. installing from a tarball
                chan_name = solv::ObjRepoViewConst::of_solvable(s).name();
                assert(chan_name != "__explicit_specs__");
            }

            r.push_back(
                {name,
                 printers::FormattedString(std::string(s.version())),
                 printers::FormattedString(std::string(s.build_string())),
                 printers::FormattedString(cut_repo_name(chan_name)),
                 dlsize_s}
            );
        };

        const auto& pool = m_pool.pool();

        trans().classify_for_each_type(
            pool,
            [&](const auto type, const auto& solv_ids)
            {
                for (const solv::SolvableId id : solv_ids)
                {
                    const auto s = [&]()
                    {
                        auto maybe_s = pool.get_solvable(id);
                        assert(maybe_s.has_value());
                        return maybe_s.value();
                    }();
                    auto get_newer = [&]()
                    {
                        auto maybe_newer_id = trans().step_newer(pool, id);
                        assert(maybe_newer_id.has_value());
                        auto maybe_newer = pool.get_solvable(maybe_newer_id.value());
                        assert(maybe_newer.has_value());
                        return maybe_newer.value();
                    };

                    if (filter(s))
                    {
                        format_row(ignored, s, Status::ignore, "=");
                        continue;
                    }
                    switch (type)
                    {
                        case SOLVER_TRANSACTION_UPGRADED:
                            format_row(upgraded, s, Status::remove, "-");
                            format_row(upgraded, get_newer(), Status::install, "+");
                            break;
                        case SOLVER_TRANSACTION_CHANGED:
                            format_row(changed, s, Status::remove, "-");
                            format_row(changed, get_newer(), Status::install, "+");
                            break;
                        case SOLVER_TRANSACTION_REINSTALLED:
                            format_row(reinstalled, s, Status::install, "o");
                            break;
                        case SOLVER_TRANSACTION_DOWNGRADED:
                            format_row(downgraded, s, Status::remove, "-");
                            format_row(downgraded, get_newer(), Status::install, "+");
                            break;
                        case SOLVER_TRANSACTION_ERASE:
                            format_row(erased, s, Status::remove, "-");
                            break;
                        case SOLVER_TRANSACTION_INSTALL:
                            format_row(installed, s, Status::install, "+");
                            break;
                        case SOLVER_TRANSACTION_IGNORE:
                            break;
                        case SOLVER_TRANSACTION_VENDORCHANGE:
                        case SOLVER_TRANSACTION_ARCHCHANGE:
                        default:
                            LOG_ERROR << "Print case not handled: " << type;
                            break;
                    }
                }
            },
            SOLVER_TRANSACTION_SHOW_OBSOLETES | SOLVER_TRANSACTION_OBSOLETE_IS_UPGRADE
        );

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
        t.add_row({summary.str()});
        auto out = Console::stream();
        t.print(out);
    }

    MTransaction
    create_explicit_transaction_from_urls(MPool& pool, const std::vector<std::string>& urls, MultiPackageCache& package_caches, std::vector<detail::other_pkg_mgr_spec>&)
    {
        std::vector<MatchSpec> specs_to_install;
        for (auto& u : urls)
        {
            std::string x(strip(u));
            if (x.empty())
            {
                continue;
            }

            std::size_t hash = u.find_first_of('#');
            MatchSpec ms(u.substr(0, hash), pool.channel_context());

            if (hash != std::string::npos)
            {
                std::string s_hash = u.substr(hash + 1);
                if (starts_with(s_hash, "sha256:"))
                {
                    ms.brackets["sha256"] = s_hash.substr(7);
                }
                else
                {
                    ms.brackets["md5"] = s_hash;
                }
            }
            specs_to_install.push_back(ms);
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
        const auto maybe_lockfile = read_environment_lockfile(pool.channel_context(), env_lockfile_path);
        if (!maybe_lockfile)
        {
            throw maybe_lockfile.error();  // NOTE: we cannot return an `un/expected` because
                                           // MTransaction is not move-enabled.
        }

        const auto lockfile_data = maybe_lockfile.value();

        struct
        {
            std::vector<PackageInfo> conda, pip;
        } packages;

        for (const auto& category : categories)
        {
            std::vector<PackageInfo> selected_packages;

            selected_packages = lockfile_data.get_packages_for(
                category,
                Context::instance().platform,
                "conda"
            );
            std::copy(
                selected_packages.begin(),
                selected_packages.end(),
                std::back_inserter(packages.conda)
            );

            if (selected_packages.empty())
            {
                LOG_WARNING << "Selected packages for category '" << category << "' are empty. "
                            << "The lockfile might not be resolved for your platform ("
                            << Context::instance().platform << ").";
            }

            selected_packages = lockfile_data
                                    .get_packages_for(category, Context::instance().platform, "pip");
            std::copy(
                selected_packages.begin(),
                selected_packages.end(),
                std::back_inserter(packages.pip)
            );
        }

        // extract pip packages
        if (!packages.pip.empty())
        {
            std::vector<std::string> pip_specs;
            for (const auto& package : packages.pip)
            {
                pip_specs.push_back(package.name + " @ " + package.url + "#sha256=" + package.sha256);
            }
            other_specs.push_back(
                {"pip --no-deps", pip_specs, fs::absolute(env_lockfile_path.parent_path()).string()}
            );
        }

        return MTransaction{pool, packages.conda, package_caches};
    }

}  // namespace mamba
