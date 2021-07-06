// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <stack>
#include <thread>

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/link.hpp"
#include "mamba/core/match_spec.hpp"
#include "mamba/core/thread_utils.hpp"

namespace
{
    bool need_pkg_download(const mamba::PackageInfo& pkg_info,
                           mamba::MultiPackageCache& cache,
                           const fs::path& cache_path)
    {
        // See PackageDownloadExtractTarget::target() for note about when we need to download
        // the package.
        bool need_download(cache.first_cache_path(pkg_info).empty());
        if (need_download)
            need_download = cache.query(pkg_info) != cache_path;
        return need_download;
    }
}  // anonymouse namspace

namespace mamba
{
    nlohmann::json solvable_to_json(Solvable* s)
    {
        return PackageInfo(s).json();
    }

    /********************************
     * PackageDownloadExtractTarget *
     ********************************/

    std::mutex PackageDownloadExtractTarget::extract_mutex;

    static std::mutex lookup_checksum_mutex;
    std::string lookup_checksum(Solvable* s, Id checksum_type)
    {
        Id unused;
        std::lock_guard<std::mutex> lock(lookup_checksum_mutex);
        std::string chksum = check_char(solvable_lookup_checksum(s, checksum_type, &unused));
        return chksum;
    }

    PackageDownloadExtractTarget::PackageDownloadExtractTarget(Solvable* solvable)
        : PackageDownloadExtractTarget(PackageInfo(solvable))
    {
    }

    PackageDownloadExtractTarget::PackageDownloadExtractTarget(const PackageInfo& pkg_info)
        : m_finished(false)
        , m_package_info(pkg_info)
    {
        m_filename = pkg_info.fn;
        m_channel = pkg_info.channel;
        m_url = pkg_info.url;
        m_name = pkg_info.name;

        m_expected_size = pkg_info.size;
        m_sha256 = pkg_info.sha256;
        m_md5 = pkg_info.md5;
    }

    void PackageDownloadExtractTarget::write_repodata_record(const fs::path& base_path)
    {
        fs::path repodata_record_path = base_path / "info" / "repodata_record.json";
        fs::path index_path = base_path / "info" / "index.json";

        nlohmann::json index, solvable_json;
        std::ifstream index_file(index_path);
        index_file >> index;

        solvable_json = m_package_info.json();
        index.insert(solvable_json.cbegin(), solvable_json.cend());

        std::ofstream repodata_record(repodata_record_path);
        repodata_record << index.dump(4);
    }

    void PackageDownloadExtractTarget::add_url()
    {
        std::ofstream urls_txt(m_cache_path / "urls.txt", std::ios::app);
        urls_txt << m_url << std::endl;
    }

    void PackageDownloadExtractTarget::validate()
    {
        m_validation_result = VALIDATION_RESULT::VALID;
        if (m_expected_size && size_t(m_target->downloaded_size) != m_expected_size)
        {
            LOG_ERROR << "File not valid: file size doesn't match expectation " << m_tarball_path
                      << "\nExpected: " << m_expected_size << "\n";
            m_progress_proxy.mark_as_completed("File size validation error.");
            m_validation_result = SIZE_ERROR;
        }
        interruption_point();

        if (!m_sha256.empty() && !validate::sha256(m_tarball_path, m_sha256))
        {
            m_validation_result = SHA256_ERROR;
            m_progress_proxy.mark_as_completed("SHA256 sum validation error.");
            LOG_ERROR << "File not valid: SHA256 sum doesn't match expectation " << m_tarball_path
                      << "\nExpected: " << m_sha256 << "\n";
        }
        else
        {
            if (!m_md5.empty() && !validate::md5(m_tarball_path, m_md5))
            {
                m_validation_result = MD5SUM_ERROR;
                m_progress_proxy.mark_as_completed("MD5 sum validation error.");
                LOG_ERROR << "File not valid: MD5 sum doesn't match expectation " << m_tarball_path
                          << "\nExpected: " << m_md5 << "\n";
            }
        }
    }

    bool PackageDownloadExtractTarget::extract()
    {
        // Extracting is __not__ yet thread safe it seems...
        interruption_point();
        LOG_INFO << "Waiting for decompression " << m_tarball_path;
        m_progress_proxy.set_postfix("Waiting...");
        {
            std::lock_guard<std::mutex> lock(PackageDownloadExtractTarget::extract_mutex);
            interruption_point();
            m_progress_proxy.set_postfix("Decompressing...");
            LOG_INFO << "Decompressing " << m_tarball_path;
            fs::path extract_path;
            try
            {
                extract_path = mamba::extract(m_tarball_path);
                interruption_point();
                LOG_INFO << "Extracted to " << extract_path;
                write_repodata_record(extract_path);
                add_url();
                m_progress_proxy.mark_as_completed("Extraction successful");
            }
            catch (std::exception& e)
            {
                LOG_ERROR << "Error when extracting package: " << e.what();
                m_decompress_exception = e;
                m_validation_result = VALIDATION_RESULT::EXTRACT_ERROR;
                m_finished = true;
                m_progress_proxy.mark_as_completed("Extraction error");
                return false;
            }
        }

        m_finished = true;
        return m_finished;
    }

    bool PackageDownloadExtractTarget::extract_from_cache()
    {
        bool result = this->extract();
        if (result)
        {
            std::stringstream final_msg;
            final_msg << "Extracted " << std::left << std::setw(30) << m_name;
            m_progress_proxy.mark_as_completed(final_msg.str());
            return result;
        }
        // currently we always return true
        return true;
    }

    bool PackageDownloadExtractTarget::validate_extract()
    {
        validate();
        // Validation
        if (m_validation_result != VALIDATION_RESULT::VALID)
        {
            // abort here, but set finished to true
            m_finished = true;
            return true;
        }

        std::stringstream final_msg;
        final_msg << "Finished " << std::left << std::setw(30) << m_name << std::right
                  << std::setw(8);
        m_progress_proxy.elapsed_time_to_stream(final_msg);
        final_msg << " " << std::setw(12 + 2);
        to_human_readable_filesize(final_msg, m_expected_size);
        final_msg << " " << std::setw(6);
        to_human_readable_filesize(final_msg, m_target->avg_speed);
        final_msg << "/s";
        m_progress_proxy.mark_as_completed(final_msg.str());

        bool result = this->extract();
        m_progress_proxy.mark_as_extracted();
        return result;
    }

    bool PackageDownloadExtractTarget::finalize_callback()
    {
        m_progress_proxy.set_full();
        m_progress_proxy.set_postfix("Validating...");

        LOG_INFO << "Download finished, validating " << m_tarball_path;

        thread v(&PackageDownloadExtractTarget::validate_extract, this);
        v.detach();

        return true;
    }

    bool PackageDownloadExtractTarget::finished()
    {
        return m_finished;
    }

    auto PackageDownloadExtractTarget::validation_result() const
    {
        return m_validation_result;
    }

    void PackageDownloadExtractTarget::clear_cache() const
    {
        fs::remove_all(m_tarball_path);
        fs::path dest_dir = strip_package_extension(m_tarball_path);
        if (fs::exists(dest_dir))
        {
            fs::remove_all(dest_dir);
        }
    }

    const std::string& PackageDownloadExtractTarget::name() const
    {
        return m_name;
    }

    // todo remove cache from this interface
    DownloadTarget* PackageDownloadExtractTarget::target(const fs::path& cache_path,
                                                         MultiPackageCache& cache)
    {
        //
        // tarball can be removed, it's fine if only the correct dest dir exists
        // 1. If there is extracted cache, use it, otherwise next.
        // 2. If there is tarball in `cache_path` (writable), extract it, otherwise next.
        // 3. Run the full download pipeline.
        //
        m_cache_path = cache_path;
        fs::path pkg_cache_path(cache.first_cache_path(m_package_info));
        if (pkg_cache_path.empty())
        {
            pkg_cache_path = cache.query(m_package_info);
            if (cache_path == pkg_cache_path)
            {
                m_tarball_path = pkg_cache_path / m_filename;
                m_progress_proxy = Console::instance().add_progress_bar(m_name);
                m_validation_result = VALIDATION_RESULT::VALID;
                thread v(&PackageDownloadExtractTarget::extract_from_cache, this);
                v.detach();
            }
            else
            {
                m_tarball_path = cache_path / m_filename;
                cache.clear_query_cache(m_package_info);
                // need to download this file
                LOG_INFO << "Adding " << m_name << " with " << m_url;

                m_progress_proxy = Console::instance().add_progress_bar(m_name, m_expected_size);
                m_target = std::make_unique<DownloadTarget>(m_name, m_url, cache_path / m_filename);
                m_target->set_finalize_callback(&PackageDownloadExtractTarget::finalize_callback,
                                                this);
                m_target->set_expected_size(m_expected_size);
                m_target->set_progress_bar(m_progress_proxy);
                return m_target.get();
            }
        }
        LOG_INFO << "Using cache " << m_name;
        m_finished = true;
        return nullptr;
    }

    /*******************************
     * MTransaction implementation *
     *******************************/

    bool MTransaction::filter(Solvable* s)
    {
        if (m_filter_type == FilterType::none)
            return false;
        bool spec_in_filter = m_filter_name_ids.count(s->name);

        if (m_filter_type == FilterType::keep_only)
        {
            return spec_in_filter;
        }
        else
        {
            return !spec_in_filter;
        }
    }

    MTransaction::MTransaction(MSolver& solver,
                               MultiPackageCache& cache,
                               const std::string& cache_dir)
        : m_multi_cache(cache)
        , m_cache_path(cache_dir)
    {
        if (!solver.is_solved())
        {
            throw std::runtime_error(
                "Cannot create transaction without calling solver.solve() first.");
        }

        m_transaction = solver_create_transaction(solver);
        transaction_order(m_transaction, 0);

        auto* pool = static_cast<Solver*>(solver)->pool;

        m_history_entry = History::UserRequest::prefilled();

        if (solver.no_deps || solver.only_deps)
        {
            m_filter_type = solver.only_deps ? FilterType::keep_only : FilterType::ignore;
            for (auto& s : solver.install_specs())
            {
                m_filter_name_ids.insert(pool_str2id(pool, s.name.c_str(), 0));
            }
            for (auto& s : solver.remove_specs())
            {
                m_filter_name_ids.insert(pool_str2id(pool, s.name.c_str(), 0));
            }

            if (solver.only_deps)
            {
                Queue q;
                queue_init(&q);
                transaction_installedresult(m_transaction, &q);
                for (int i = 0; i < q.count; ++i)
                {
                    Solvable* s = pool_id2solvable(pool, q.elements[i]);
                    if (m_filter_name_ids.count(s->name))
                    {
                        // add the dependencies of this selected package to the added specs
                        Id* reqp;
                        for (reqp = s->repo->idarraydata + s->requires; *reqp; reqp++)
                        {
                            const char* depname = pool_id2str(pool, *reqp);
                            const char* depevr = pool_id2evr(pool, *reqp);
                            std::string add_spec;
                            if (depname)
                            {
                                add_spec += depname;
                                if (depevr && strlen(depevr))
                                {
                                    add_spec += " ";
                                    add_spec += depevr;
                                }
                            }
                            m_history_entry.update.push_back(MatchSpec(add_spec).str());
                        }
                    }
                }
                queue_free(&q);
            }
        }

        if (solver.only_deps == false)
        {
            auto to_string_vec = [](const std::vector<MatchSpec>& vec) -> std::vector<std::string> {
                std::vector<std::string> res;
                for (const auto& el : vec)
                    res.push_back(el.str());
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
            JsonLogger::instance().json_down("actions");
            JsonLogger::instance().json_write({ { "PREFIX", Context::instance().target_prefix } });
        }
    }

    MTransaction::~MTransaction()
    {
        LOG_INFO << "Freeing transaction.";
        transaction_free(m_transaction);
    }

    void MTransaction::init()
    {
        for (int i = 0; i < m_transaction->steps.count && !is_sig_interrupted(); i++)
        {
            Id p = m_transaction->steps.elements[i];
            Id ttype = transaction_type(m_transaction, p, SOLVER_TRANSACTION_SHOW_ALL);
            Solvable* s = pool_id2solvable(m_transaction->pool, p);

            if (filter(s))
            {
                continue;
            }

            switch (ttype)
            {
                case SOLVER_TRANSACTION_DOWNGRADED:
                case SOLVER_TRANSACTION_UPGRADED:
                case SOLVER_TRANSACTION_CHANGED:
                case SOLVER_TRANSACTION_REINSTALLED:
                {
                    if (ttype == SOLVER_TRANSACTION_REINSTALLED && m_force_reinstall == false)
                        break;

                    m_to_remove.push_back(s);
                    m_to_install.push_back(m_transaction->pool->solvables
                                           + transaction_obs_pkg(m_transaction, p));
                    break;
                }
                case SOLVER_TRANSACTION_ERASE:
                {
                    m_to_remove.push_back(s);
                    break;
                }
                case SOLVER_TRANSACTION_INSTALL:
                {
                    m_to_install.push_back(s);
                    break;
                }
                case SOLVER_TRANSACTION_IGNORE:
                    break;
                default:
                    LOG_ERROR << "Exec case not handled: " << ttype;
                    break;
            }
        }
    }

    std::string MTransaction::find_python_version()
    {
        // We need to find the python version that will be there after this
        // Transaction is finished in order to compile the noarch packages correctly,
        // for example
        Pool* pool = m_transaction->pool;
        assert(pool != nullptr);

        std::string py_ver;
        Id python = pool_str2id(pool, "python", 0);

        for (Solvable* s : m_to_install)
        {
            if (s->name == python)
            {
                py_ver = pool_id2str(pool, s->evr);
                LOG_INFO << "Found python version in packages to be installed " << py_ver;
                return py_ver;
            }
        }
        if (pool->installed != nullptr)
        {
            Id p;
            Solvable* s;

            FOR_REPO_SOLVABLES(pool->installed, p, s)
            {
                if (s->name == python)
                {
                    py_ver = pool_id2str(pool, s->evr);
                    LOG_INFO << "Found python in installed packages " << py_ver;
                    break;
                }
            }
        }
        if (py_ver.size())
        {
            // we need to make sure that we're not about to remove python!
            for (Solvable* s : m_to_remove)
            {
                if (s->name == python)
                    return "";
            }
        }
        return py_ver;
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
            JsonLogger::instance().json_up();
        JsonLogger::instance().json_write(
            { { "dry_run", ctx.dry_run }, { "prefix", ctx.target_prefix } });
        if (empty())
            JsonLogger::instance().json_write(
                { { "message", "All requested packages already installed" } });
        // finally, print the JSON
        if (ctx.json)
            Console::instance().print(JsonLogger::instance().json_log.unflatten().dump(4), true);

        if (ctx.dry_run)
        {
            Console::stream() << "Dry run. Not executing transaction.";
            return true;
        }

        Console::stream() << "\n\nTransaction starting";
        m_transaction_context = TransactionContext(prefix.path(), find_python_version());
        History::UserRequest ur = History::UserRequest::prefilled();

        TransactionRollback rollback;

        auto* pool = m_transaction->pool;

        for (int i = 0; i < m_transaction->steps.count && !is_sig_interrupted(); i++)
        {
            Id p = m_transaction->steps.elements[i];
            Id ttype = transaction_type(m_transaction, p, SOLVER_TRANSACTION_SHOW_ALL);
            Solvable* s = pool_id2solvable(pool, p);

            if (filter(s))
            {
                continue;
            }

            switch (ttype)
            {
                case SOLVER_TRANSACTION_DOWNGRADED:
                case SOLVER_TRANSACTION_UPGRADED:
                case SOLVER_TRANSACTION_CHANGED:
                case SOLVER_TRANSACTION_REINSTALLED:
                {
                    if (ttype == SOLVER_TRANSACTION_REINSTALLED && m_force_reinstall == false)
                        break;

                    Solvable* s2
                        = m_transaction->pool->solvables + transaction_obs_pkg(m_transaction, p);
                    Console::stream()
                        << "Changing " << PackageInfo(s).str() << " ==> " << PackageInfo(s2).str();
                    PackageInfo p_unlink(s);
                    const fs::path ul_cache_path(m_multi_cache.first_cache_path(p_unlink));
                    PackageInfo p_link(s2);
                    const fs::path l_cache_path(m_multi_cache.first_cache_path(p_link, false));

                    UnlinkPackage up(p_unlink,
                                     ul_cache_path.empty() ? m_cache_path : ul_cache_path,
                                     &m_transaction_context);
                    up.execute();
                    rollback.record(up);

                    LinkPackage lp(p_link, l_cache_path, &m_transaction_context);
                    lp.execute();
                    rollback.record(lp);

                    m_history_entry.unlink_dists.push_back(p_unlink.long_str());
                    m_history_entry.link_dists.push_back(p_link.long_str());

                    break;
                }
                case SOLVER_TRANSACTION_ERASE:
                {
                    PackageInfo p(s);
                    Console::stream() << "Unlinking " << p.str();
                    const fs::path cache_path(m_multi_cache.first_cache_path(p));
                    UnlinkPackage up(
                        p, cache_path.empty() ? m_cache_path : cache_path, &m_transaction_context);
                    up.execute();
                    rollback.record(up);
                    m_history_entry.unlink_dists.push_back(p.long_str());
                    break;
                }
                case SOLVER_TRANSACTION_INSTALL:
                {
                    PackageInfo p(s);
                    Console::stream() << "Linking " << p.str();
                    const fs::path cache_path(m_multi_cache.first_cache_path(p, false));
                    LinkPackage lp(p, cache_path, &m_transaction_context);
                    lp.execute();
                    rollback.record(lp);
                    m_history_entry.link_dists.push_back(p.long_str());
                    break;
                }
                case SOLVER_TRANSACTION_IGNORE:
                    break;
                default:
                    LOG_ERROR << "Exec case not handled: " << ttype;
                    break;
            }
        }

        bool interrupted = is_sig_interrupted();
        if (interrupted)
        {
            Console::stream() << "Transaction interrupted, rollbacking";
            rollback.rollback();
        }
        else
        {
            Console::stream() << "Transaction finished";
            prefix.history().add_entry(m_history_entry);
        }
        return !interrupted;
    }

    auto MTransaction::to_conda() -> to_conda_type
    {
        Id real_repo_key = pool_str2id(m_transaction->pool, "solvable:real_repo_url", 1);

        to_install_type to_install_structured;
        to_remove_type to_remove_structured;

        for (Solvable* s : m_to_remove)
        {
            const char* mediafile = solvable_lookup_str(s, SOLVABLE_MEDIAFILE);
            to_remove_structured.emplace_back(s->repo->name, mediafile);
        }

        for (Solvable* s : m_to_install)
        {
            const char* mediafile = solvable_lookup_str(s, SOLVABLE_MEDIAFILE);
            std::string s_json = solvable_to_json(s).dump(4);

            std::string channel;
            if (solvable_lookup_str(s, real_repo_key))
            {
                channel = solvable_lookup_str(s, real_repo_key);
            }
            else
            {
                channel = s->repo->name;  // note this can and should be <unknown> when
                                          // e.g. installing from a tarball
            }

            to_install_structured.emplace_back(channel, mediafile, s_json);
        }

        to_specs_type specs;
        std::get<0>(specs) = m_history_entry.update;
        std::get<1>(specs) = m_history_entry.remove;

        return std::make_tuple(specs, to_install_structured, to_remove_structured);
    }

    void MTransaction::log_json()
    {
        std::vector<nlohmann::json> to_fetch, to_link, to_unlink;

        for (Solvable* s : m_to_install)
        {
            if (!need_pkg_download(s, m_multi_cache, m_cache_path))
            {
                to_link.push_back(solvable_to_json(s));
            }
            else
            {
                to_fetch.push_back(solvable_to_json(s));
                to_link.push_back(solvable_to_json(s));
            }
        }

        for (Solvable* s : m_to_remove)
        {
            to_unlink.push_back(solvable_to_json(s));
        }

        auto add_json = [](const auto& jlist, const char* s) {
            if (!jlist.empty())
            {
                JsonLogger::instance().json_down(s);
                for (nlohmann::json j : jlist)
                {
                    JsonLogger::instance().json_append(j);
                }
                JsonLogger::instance().json_up();
            }
        };

        add_json(to_fetch, "FETCH");
        add_json(to_link, "LINK");
        add_json(to_unlink, "UNLINK");
    }

    bool MTransaction::fetch_extract_packages(std::vector<MRepo*>& repos)
    {
        std::vector<std::unique_ptr<PackageDownloadExtractTarget>> targets;
        MultiDownloadTarget multi_dl;

        Console::instance().init_multi_progress(ProgressBarMode::aggregated);

        for (auto& s : m_to_install)
        {
            std::string url;
            MRepo* mamba_repo = nullptr;
            for (auto& r : repos)
            {
                if (r->repo() == s->repo)
                {
                    mamba_repo = r;
                    break;
                }
            }
            if (mamba_repo == nullptr)
            {
                throw std::runtime_error("Repo not associated.");
            }
            if (mamba_repo->url() == "")
            {
                // TODO: comment which use case it represents
                continue;
            }

            auto& ctx = Context::instance();
            if (ctx.experimental && ctx.verify_artifacts)
            {
                const auto& repo_checker = make_channel(mamba_repo->url()).repo_checker();

                auto pkg_info = PackageInfo(s);

                // TODO: avoid parsing again the index file by storing
                // keyid/signatures into libsolv Solvable
                repo_checker.verify_package(fs::path(mamba_repo->index_file()),
                                            pkg_info.str() + ".tar.bz2");

                LOG_DEBUG << "Package '" << pkg_info.name << "' trusted from channel '"
                          << mamba_repo->url() << "' metadata";
            }

            targets.emplace_back(std::make_unique<PackageDownloadExtractTarget>(s));
            multi_dl.add(targets[targets.size() - 1]->target(m_cache_path, m_multi_cache));
        }

        interruption_guard g([]() { Console::instance().init_multi_progress(); });

        bool downloaded = multi_dl.download(true);
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

        for (const auto& t : targets)
        {
            if (t->validation_result() != PackageDownloadExtractTarget::VALIDATION_RESULT::VALID
                && t->validation_result()
                       != PackageDownloadExtractTarget::VALIDATION_RESULT::UNDEFINED)
            {
                t->clear_cache();
                all_valid = false;
                throw std::runtime_error("Found incorrect download. Aborting");
            }
        }

        return !is_sig_interrupted() && downloaded && all_valid;
    }

    bool MTransaction::empty()
    {
        return m_to_install.size() == 0 && m_to_remove.size() == 0;
    }

    bool MTransaction::prompt(std::vector<MRepo*>& repos)
    {
        print();
        if (Context::instance().dry_run || empty())
        {
            return true;
        }

        bool res = Console::prompt("Confirm changes", 'y');
        if (res)
        {
            return fetch_extract_packages(repos);
        }
        return res;
    }

    void MTransaction::print()
    {
        if (Context::instance().json)
            return;

        Console::print("Transaction\n");
        Console::stream() << "  Prefix: " << Context::instance().target_prefix.string() << "\n";

        // check size of transaction
        if (empty())
        {
            if (m_history_entry.update.size())
            {
                Console::print("  All requested packages already installed\n");
            }
            else
            {
                Console::print("  Nothing to do\n");
            }
            return;
        }

        if (m_history_entry.update.size())
        {
            Console::print("  Updating specs:\n");
            for (auto& s : m_history_entry.update)
            {
                Console::stream() << "   - " << s;
            }
        }

        if (m_history_entry.remove.size())
        {
            Console::print("  Removing specs:\n");
            for (auto& s : m_history_entry.remove)
            {
                Console::stream() << "   - " << s;
            }
        }
        Console::stream() << "\n";
        if (m_history_entry.update.empty() && m_history_entry.remove.empty())
        {
            Console::print("  No specs added or removed.\n");
        }

        printers::Table t({ "Package", "Version", "Build", "Channel", "Size" });
        t.set_alignment({ printers::alignment::left,
                          printers::alignment::right,
                          printers::alignment::left,
                          printers::alignment::left,
                          printers::alignment::right });
        t.set_padding({ 2, 2, 2, 2, 5 });
        Queue classes, pkgs;

        queue_init(&classes);
        queue_init(&pkgs);

        using rows = std::vector<std::vector<printers::FormattedString>>;

        rows downgraded, upgraded, changed, erased, installed, ignored;
        std::size_t total_size = 0;
        auto* pool = m_transaction->pool;

        auto format_row = [this, pool, &total_size](
                              rows& r, Solvable* s, printers::format flag, std::string diff) {
            std::ptrdiff_t dlsize = solvable_lookup_num(s, SOLVABLE_DOWNLOADSIZE, -1);
            printers::FormattedString dlsize_s;
            if (dlsize != -1)
            {
                if (static_cast<std::size_t>(flag)
                    & static_cast<std::size_t>(printers::format::yellow))
                {
                    dlsize_s.s = "Ignored";
                }
                else
                {
                    if (!need_pkg_download(s, m_multi_cache, m_cache_path))
                    {
                        dlsize_s.s = "Cached";
                        dlsize_s.flag = printers::format::green;
                    }
                    else
                    {
                        std::stringstream s;
                        to_human_readable_filesize(s, dlsize);
                        dlsize_s.s = s.str();
                        // Hacky hacky
                        if (static_cast<std::size_t>(flag)
                            & static_cast<std::size_t>(printers::format::green))
                        {
                            total_size += dlsize;
                        }
                    }
                }
            }
            printers::FormattedString name;
            name.s = diff + " " + std::string(pool_id2str(pool, s->name));
            name.flag = flag;
            const char* build_string = solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR);

            std::string channel;
            Id real_repo_key = pool_str2id(pool, "solvable:real_repo_url", 1);
            if (solvable_lookup_str(s, real_repo_key))
            {
                channel = solvable_lookup_str(s, real_repo_key);
            }
            else
            {
                channel = s->repo->name;  // note this can and should be <unknown> when
                                          // e.g. installing from a tarball
            }

            r.push_back({ name,
                          printers::FormattedString(pool_id2str(pool, s->evr)),
                          printers::FormattedString(build_string ? build_string : ""),
                          printers::FormattedString(cut_repo_name(channel)),
                          dlsize_s });
        };

        int mode = SOLVER_TRANSACTION_SHOW_OBSOLETES | SOLVER_TRANSACTION_OBSOLETE_IS_UPGRADE;
        transaction_classify(m_transaction, mode, &classes);
        Id cls;
        for (int i = 0; i < classes.count; i += 4)
        {
            cls = classes.elements[i];
            transaction_classify_pkgs(
                m_transaction, mode, cls, classes.elements[i + 2], classes.elements[i + 3], &pkgs);

            for (int j = 0; j < pkgs.count; j++)
            {
                Id p = pkgs.elements[j];
                Solvable* s = m_transaction->pool->solvables + p;

                if (filter(s))
                {
                    format_row(ignored, s, printers::format::yellow, "=");
                    continue;
                }
                switch (cls)
                {
                    case SOLVER_TRANSACTION_UPGRADED:
                        format_row(upgraded, s, printers::format::red, "-");
                        format_row(upgraded,
                                   m_transaction->pool->solvables
                                       + transaction_obs_pkg(m_transaction, p),
                                   printers::format::green,
                                   "+");
                        break;
                    case SOLVER_TRANSACTION_CHANGED:
                    case SOLVER_TRANSACTION_REINSTALLED:
                        if (cls == SOLVER_TRANSACTION_REINSTALLED && m_force_reinstall == false)
                            break;

                        format_row(changed, s, printers::format::red, "-");
                        format_row(changed,
                                   m_transaction->pool->solvables
                                       + transaction_obs_pkg(m_transaction, p),
                                   printers::format::green,
                                   "+");
                        break;
                    case SOLVER_TRANSACTION_DOWNGRADED:
                        format_row(downgraded, s, printers::format::red, "-");
                        format_row(downgraded,
                                   m_transaction->pool->solvables
                                       + transaction_obs_pkg(m_transaction, p),
                                   printers::format::green,
                                   "+");
                        break;
                    case SOLVER_TRANSACTION_ERASE:
                        format_row(erased, s, printers::format::red, "-");
                        break;
                    case SOLVER_TRANSACTION_INSTALL:
                        format_row(installed, s, printers::format::green, "+");
                        break;
                    case SOLVER_TRANSACTION_IGNORE:
                        break;
                    case SOLVER_TRANSACTION_VENDORCHANGE:
                    case SOLVER_TRANSACTION_ARCHCHANGE:
                    default:
                        LOG_ERROR << "Print case not handled: " << cls;
                        break;
                }
            }
        }

        queue_free(&classes);
        queue_free(&pkgs);

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
        to_human_readable_filesize(summary, total_size);
        summary << "\n";
        t.add_row({ summary.str() });
        t.print(std::cout);
    }
}  // namespace mamba
