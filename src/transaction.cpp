// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <stack>
#include <thread>

#include "mamba/transaction.hpp"
#include "mamba/link.hpp"
#include "mamba/match_spec.hpp"
#include "mamba/thread_utils.hpp"

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

    bool PackageDownloadExtractTarget::validate_extract()
    {
        // Validation
        if (m_expected_size && size_t(m_target->downloaded_size) != m_expected_size)
        {
            LOG_ERROR << "File not valid: file size doesn't match expectation " << m_tarball_path;
            throw std::runtime_error("File not valid: file size doesn't match expectation ("
                                     + std::string(m_tarball_path) + ")");
        }
        interruption_point();

        if (!m_sha256.empty() && !validate::sha256(m_tarball_path, m_sha256))
        {
            LOG_ERROR << "File not valid: SHA256 sum doesn't match expectation " << m_tarball_path;
            throw std::runtime_error("File not valid: SHA256 sum doesn't match expectation ("
                                     + std::string(m_tarball_path) + ")");
        }
        else
        {
            if (!m_md5.empty() && !validate::md5(m_tarball_path, m_md5))
            {
                LOG_ERROR << "File not valid: MD5 sum doesn't match expectation " << m_tarball_path;
                throw std::runtime_error("File not valid: MD5 sum doesn't match expectation ("
                                         + std::string(m_tarball_path) + ")");
            }
        }

        interruption_point();
        LOG_INFO << "Waiting for decompression " << m_tarball_path;
        m_progress_proxy.set_postfix("Waiting...");
        // Extract path is __not__ yet thread safe it seems...
        {
            std::lock_guard<std::mutex> lock(PackageDownloadExtractTarget::extract_mutex);
            interruption_point();
            m_progress_proxy.set_postfix("Decompressing...");
            LOG_INFO << "Decompressing " << m_tarball_path;
            auto extract_path = extract(m_tarball_path);
            LOG_INFO << "Extracted to " << extract_path;
            write_repodata_record(extract_path);
            add_url();
        }

        interruption_point();
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

        m_finished = true;
        return m_finished;
    }

    bool PackageDownloadExtractTarget::finalize_callback()
    {
        m_progress_proxy.set_progress(100);
        m_progress_proxy.set_postfix("Validating...");

        LOG_INFO << "Download finished, validating " << m_tarball_path;

        thread v(&PackageDownloadExtractTarget::validate_extract, this);
        v.detach();

        return true;
    }

    bool PackageDownloadExtractTarget::finished()
    {
        return m_target == nullptr ? true : m_finished;
    }

    // todo remove cache from this interface
    DownloadTarget* PackageDownloadExtractTarget::target(const fs::path& cache_path,
                                                         MultiPackageCache& cache)
    {
        m_cache_path = cache_path;
        m_tarball_path = cache_path / m_filename;
        fs::path dest_dir = strip_package_extension(m_tarball_path);
        bool dest_dir_exists = fs::exists(dest_dir);

        bool valid = cache.query(m_package_info);

        if (valid && !dest_dir_exists)
        {
            // TODO add extract job here
        }

        // tarball can be removed, it's fine if only the correct dest dir exists
        if (!valid)
        {
            // need to download this file
            LOG_INFO << "Adding " << m_name << " with " << m_url;

            m_progress_proxy = Console::instance().add_progress_bar(m_name);
            m_target = std::make_unique<DownloadTarget>(m_name, m_url, cache_path / m_filename);
            m_target->set_finalize_callback(&PackageDownloadExtractTarget::finalize_callback, this);
            m_target->set_expected_size(m_expected_size);
            m_target->set_progress_bar(m_progress_proxy);
        }
        else
        {
            LOG_INFO << "Using cache " << m_name;
        }
        return m_target.get();
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

    MTransaction::MTransaction(MSolver& solver, MultiPackageCache& cache)
        : m_multi_cache(cache)
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
        Queue classes, pkgs;

        queue_init(&classes);
        queue_init(&pkgs);

        int mode = SOLVER_TRANSACTION_SHOW_OBSOLETES | SOLVER_TRANSACTION_OBSOLETE_IS_UPGRADE;

        transaction_classify(m_transaction, mode, &classes);

        Id cls;
        for (int i = 0; i < classes.count; i += 4)
        {
            cls = classes.elements[i];
            // cnt = classes.elements[i + 1];

            transaction_classify_pkgs(
                m_transaction, mode, cls, classes.elements[i + 2], classes.elements[i + 3], &pkgs);
            for (int j = 0; j < pkgs.count; j++)
            {
                Id p = pkgs.elements[j];
                Solvable* s = m_transaction->pool->solvables + p;

                if (filter(s))
                {
                    continue;
                }

                switch (cls)
                {
                    case SOLVER_TRANSACTION_DOWNGRADED:
                    case SOLVER_TRANSACTION_UPGRADED:
                    case SOLVER_TRANSACTION_CHANGED:
                    case SOLVER_TRANSACTION_REINSTALLED:
                        if (cls == SOLVER_TRANSACTION_REINSTALLED && m_force_reinstall == false)
                            break;
                        m_to_remove.push_back(s);
                        m_to_install.push_back(m_transaction->pool->solvables
                                               + transaction_obs_pkg(m_transaction, p));
                        break;
                    case SOLVER_TRANSACTION_ERASE:
                        m_to_remove.push_back(s);
                        break;
                    case SOLVER_TRANSACTION_INSTALL:
                        m_to_install.push_back(s);
                        break;
                    case SOLVER_TRANSACTION_IGNORE:
                        break;
                    case SOLVER_TRANSACTION_VENDORCHANGE:
                    case SOLVER_TRANSACTION_ARCHCHANGE:
                    default:
                        LOG_WARNING << "Print case not handled: " << cls;
                        break;
                }
            }
        }

        queue_free(&classes);
        queue_free(&pkgs);
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

    bool MTransaction::execute(PrefixData& prefix, const fs::path& cache_dir)
    {
        // JSON output
        // back to the top level if any action was required
        if (!empty())
            JsonLogger::instance().json_up();
        JsonLogger::instance().json_write({ { "dry_run", Context::instance().dry_run },
                                            { "prefix", Context::instance().target_prefix } });
        if (empty())
            JsonLogger::instance().json_write(
                { { "message", "All requested packages already installed" } });
        // finally, print the JSON
        if (Context::instance().json)
            Console::instance().print(JsonLogger::instance().json_log.unflatten().dump(4), true);

        if (Context::instance().dry_run)
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
                    PackageInfo p_link(s2);

                    UnlinkPackage up(p_unlink, fs::path(cache_dir), &m_transaction_context);
                    up.execute();
                    rollback.record(up);

                    LinkPackage lp(p_link, fs::path(cache_dir), &m_transaction_context);
                    lp.execute();
                    rollback.record(lp);

                    m_history_entry.unlink_dists.push_back(p_unlink.long_str());
                    m_history_entry.link_dists.push_back(p_link.long_str());

                    break;
                }
                case SOLVER_TRANSACTION_ERASE:
                {
                    PackageInfo p(s);
                    Console::stream() << "Unlinking " << PackageInfo(s).str();
                    UnlinkPackage up(p, fs::path(cache_dir), &m_transaction_context);
                    up.execute();
                    rollback.record(up);
                    m_history_entry.unlink_dists.push_back(p.long_str());
                    break;
                }
                case SOLVER_TRANSACTION_INSTALL:
                {
                    PackageInfo p(s);
                    Console::stream() << "Linking " << p.str();
                    LinkPackage lp(p, fs::path(cache_dir), &m_transaction_context);
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
        std::vector<nlohmann::json> to_fetch;
        std::vector<nlohmann::json> to_link;

        for (Solvable* s : m_to_install)
        {
            if (this->m_multi_cache.query(s))
            {
                to_link.push_back(solvable_to_json(s));
            }
            else
            {
                to_fetch.push_back(solvable_to_json(s));
                to_link.push_back(solvable_to_json(s));
            }
        }

        if (!to_fetch.empty())
        {
            JsonLogger::instance().json_down("FETCH");
            for (nlohmann::json j : to_fetch)
                JsonLogger::instance().json_append(j);
            JsonLogger::instance().json_up();
        }

        if (!to_link.empty())
        {
            JsonLogger::instance().json_down("LINK");
            for (nlohmann::json j : to_link)
                JsonLogger::instance().json_append(j);
            JsonLogger::instance().json_up();
        }
    }

    bool MTransaction::fetch_extract_packages(const std::string& cache_dir,
                                              std::vector<MRepo*>& repos)
    {
        fs::path cache_path(cache_dir);
        std::vector<std::unique_ptr<PackageDownloadExtractTarget>> targets;
        MultiDownloadTarget multi_dl;

        Console::instance().init_multi_progress();

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
                continue;
            }

            targets.emplace_back(std::make_unique<PackageDownloadExtractTarget>(s));
            multi_dl.add(targets[targets.size() - 1]->target(cache_path, m_multi_cache));
        }

        interruption_guard g([]() { Console::instance().init_multi_progress(); });

        bool downloaded = multi_dl.download(true);

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

        return !is_sig_interrupted() && downloaded;
    }

    bool MTransaction::empty()
    {
        return m_to_install.size() == 0 && m_to_remove.size() == 0;
    }

    bool MTransaction::prompt(const std::string& cache_dir, std::vector<MRepo*>& repos)
    {
        print();
        if (Context::instance().dry_run || empty())
        {
            return true;
        }

        bool res = Console::prompt("Confirm changes", 'y');
        if (res)
        {
            return fetch_extract_packages(cache_dir, repos);
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
                Console::print("  Nothing to remove\n");
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

        auto format_row = [this, pool, &total_size](rows& r, Solvable* s, printers::format flag) {
            std::ptrdiff_t dlsize = solvable_lookup_num(s, SOLVABLE_DOWNLOADSIZE, -1);
            printers::FormattedString dlsize_s;
            if (dlsize != -1)
            {
                if (static_cast<std::size_t>(flag)
                    & static_cast<std::size_t>(printers::format::yellow))
                {
                    dlsize_s.s = "Ignored";
                }
                else if (this->m_multi_cache.query(s))
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
            printers::FormattedString name;
            name.s = pool_id2str(pool, s->name);
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
                    format_row(ignored, s, printers::format::yellow);
                    continue;
                }
                switch (cls)
                {
                    case SOLVER_TRANSACTION_UPGRADED:
                        format_row(upgraded, s, printers::format::red);
                        format_row(upgraded,
                                   m_transaction->pool->solvables
                                       + transaction_obs_pkg(m_transaction, p),
                                   printers::format::green);
                        break;
                    case SOLVER_TRANSACTION_CHANGED:
                    case SOLVER_TRANSACTION_REINSTALLED:
                        if (cls == SOLVER_TRANSACTION_REINSTALLED && m_force_reinstall == false)
                            break;

                        format_row(changed, s, printers::format::red);
                        format_row(changed,
                                   m_transaction->pool->solvables
                                       + transaction_obs_pkg(m_transaction, p),
                                   printers::format::green);
                        break;
                    case SOLVER_TRANSACTION_DOWNGRADED:
                        format_row(downgraded, s, printers::format::red);
                        format_row(downgraded,
                                   m_transaction->pool->solvables
                                       + transaction_obs_pkg(m_transaction, p),
                                   printers::format::green);
                        break;
                    case SOLVER_TRANSACTION_ERASE:
                        format_row(erased, s, printers::format::red);
                        break;
                    case SOLVER_TRANSACTION_INSTALL:
                        format_row(installed, s, printers::format::green);
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
