#include <thread>

#include "transaction.hpp"
#include "match_spec.hpp"
#include "link.hpp"

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

    PackageDownloadExtractTarget::PackageDownloadExtractTarget(const MRepo& repo, Solvable* solvable)
        : m_solv(solvable)
        , m_finished(false)
    {
        m_filename = solvable_lookup_str(m_solv, SOLVABLE_MEDIAFILE);
        m_channel = repo.url();
        m_url = m_channel + "/" + m_filename;
        m_name = pool_id2str(m_solv->repo->pool, m_solv->name);
    }

    void PackageDownloadExtractTarget::write_repodata_record(const fs::path& base_path)
    {
        fs::path repodata_record_path = base_path / "info" / "repodata_record.json";
        fs::path index_path = base_path / "info" / "index.json";

        nlohmann::json index, solvable_json;
        std::ifstream index_file(index_path);
        index_file >> index;

        solvable_json = solvable_to_json(m_solv);

        // merge those two
        index.insert(solvable_json.cbegin(), solvable_json.cend());

        index["url"] = m_url;
        index["channel"] = m_channel;
        index["fn"] = m_filename;

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
        Id unused;

        // Validation
        auto expected_size = solvable_lookup_num(m_solv, SOLVABLE_DOWNLOADSIZE, 0);
        if (m_target->downloaded_size != expected_size)
        {
            LOG_ERROR << "File not valid: file size doesn't match expectation " << m_tarball_path;
            throw std::runtime_error("File not valid: file size doesn't match expectation (" + std::string(m_tarball_path) + ")");
        }
        const char* sha256_check = solvable_lookup_checksum(m_solv, SOLVABLE_CHECKSUM, &unused);
        if (sha256_check != nullptr && !validate::sha256(m_tarball_path, sha256_check))
        {
            LOG_ERROR << "File not valid: SHA256 sum doesn't match expectation " << m_tarball_path;
            throw std::runtime_error("File not valid: SHA256 sum doesn't match expectation (" + std::string(m_tarball_path) + ")");
        }
        else
        {
            const char* md5_check = solvable_lookup_checksum(m_solv, SOLVABLE_PKGID, &unused);
            if (md5_check != nullptr && !validate::md5(m_tarball_path, md5_check))
            {
                LOG_ERROR << "File not valid: MD5 sum doesn't match expectation " << m_tarball_path;
                throw std::runtime_error("File not valid: MD5 sum doesn't match expectation (" + std::string(m_tarball_path) + ")");
            }
        }

        LOG_INFO << "Waiting for decompression " << m_tarball_path;
        m_progress_proxy.set_postfix("Waiting...");
        // Extract path is __not__ yet thread safe it seems...
        {
            std::lock_guard<std::mutex> lock(PackageDownloadExtractTarget::extract_mutex);
            m_progress_proxy.set_postfix("Decompressing...");
            LOG_INFO << "Decompressing " << m_tarball_path;
            auto extract_path = extract(m_tarball_path);
            LOG_INFO << "Extracted to " << extract_path;
            write_repodata_record(extract_path);
            add_url();
        }

        std::stringstream final_msg;
        final_msg << "Finished " << std::left << std::setw(30) << m_name << std::right << std::setw(8);
        m_progress_proxy.elapsed_time_to_stream(final_msg);
        final_msg << " " << std::setw(12 + 2);
        to_human_readable_filesize(final_msg, expected_size);
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

        m_extract_future = std::async(std::launch::async,
                                      &PackageDownloadExtractTarget::validate_extract,
                                      this);

        return true;
    }

    bool PackageDownloadExtractTarget::finished()
    {
        return m_target == nullptr ? true : m_finished;
    }

    // todo remove cache from this interface
    DownloadTarget* PackageDownloadExtractTarget::target(const fs::path& cache_path, MultiPackageCache& cache)
    {
        m_cache_path = cache_path;
        m_tarball_path = cache_path / m_filename;
        bool tarball_exists = fs::exists(m_tarball_path);
        fs::path dest_dir = strip_package_extension(m_tarball_path);
        bool dest_dir_exists = fs::exists(dest_dir);

        bool valid = cache.query(m_solv);

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
            m_target->set_expected_size(solvable_lookup_num(m_solv, SOLVABLE_DOWNLOADSIZE, 0));
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
        if (m_filter_type == TransactionFilterType::NONE) return false;
        bool spec_in_filter = m_filter_name_ids.count(s->name);

        if (m_filter_type == TransactionFilterType::KEEP_ONLY)
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
            throw std::runtime_error("Cannot create transaction without calling solver.solve() first.");
        }

        m_transaction = solver_create_transaction(solver);
        transaction_order(m_transaction, 0);

        auto* pool = static_cast<Solver*>(solver)->pool;

        m_history_entry = History::UserRequest::prefilled();

        if (solver.no_deps || solver.only_deps)
        {
            m_filter_type = solver.only_deps ? TransactionFilterType::KEEP_ONLY : TransactionFilterType::IGNORE;
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
            auto to_string_vec = [](const std::vector<MatchSpec>& vec) -> std::vector<std::string>
            {
                std::vector<std::string> res;
                for (const auto& el : vec) res.push_back(el.str());
                return res;
            };
            m_history_entry.update = to_string_vec(solver.install_specs());
            m_history_entry.remove = to_string_vec(solver.remove_specs());
        }

        init();
        // if no action required, don't even start logging them
        if (!empty())
        {
            JsonLogger::instance().json_down("actions");
            JsonLogger::instance().json_write({{"PREFIX", Context::instance().target_prefix}});
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

        int mode = SOLVER_TRANSACTION_SHOW_OBSOLETES |
                    SOLVER_TRANSACTION_OBSOLETE_IS_UPGRADE;

        transaction_classify(m_transaction, mode, &classes);

        Id cls;
        for (int i = 0; i < classes.count; i += 4)
        {
            cls = classes.elements[i];
            // cnt = classes.elements[i + 1];

            transaction_classify_pkgs(m_transaction, mode, cls, classes.elements[i + 2],
                                      classes.elements[i + 3], &pkgs);
            for (int j = 0; j < pkgs.count; j++)
            {
                Id p = pkgs.elements[j];
                Solvable *s = m_transaction->pool->solvables + p;

                if (filter(s))
                {
                    continue;
                }

                switch (cls)
                {
                    case SOLVER_TRANSACTION_DOWNGRADED:
                    case SOLVER_TRANSACTION_UPGRADED:
                    case SOLVER_TRANSACTION_CHANGED:
                        m_to_remove.push_back(s);
                        m_to_install.push_back(m_transaction->pool->solvables + transaction_obs_pkg(m_transaction, p));
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
        // We need to find the python version that will be there after this Transaction is finished
        // in order to compile the noarch packages correctly, for example
        Pool* pool = m_transaction->pool;
        assert (pool != nullptr);

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
                if (s->name == python) return "";
            }
        }
        return py_ver;
    }

    bool MTransaction::execute(PrefixData& prefix, const fs::path& cache_dir)
    {
        Console::stream() << "\n\nTransaction starting";
        m_transaction_context = TransactionContext(prefix.path(), find_python_version());
        History::UserRequest ur = History::UserRequest::prefilled();

        auto* pool = m_transaction->pool;

        for (int i = 0; i < m_transaction->steps.count; i++)
        {
            Id p = m_transaction->steps.elements[i];
            Id ttype = transaction_type(m_transaction, p, SOLVER_TRANSACTION_SHOW_ALL);
            Solvable *s = pool_id2solvable(pool, p);

            if (filter(s))
            {
                continue;
            }

            switch (ttype)
            {
                case SOLVER_TRANSACTION_DOWNGRADED:
                case SOLVER_TRANSACTION_UPGRADED:
                case SOLVER_TRANSACTION_CHANGED:
                {
                    Solvable* s2 = m_transaction->pool->solvables + transaction_obs_pkg(m_transaction, p);
                    Console::stream() << "Changing " << PackageInfo(s).str() << " ==> " << PackageInfo(s2).str();
                    PackageInfo p_unlink(s);
                    PackageInfo p_link(s2);
                    UnlinkPackage up(PackageInfo(s), &m_transaction_context);
                    up.execute();

                    LinkPackage lp(PackageInfo(s2), fs::path(cache_dir), &m_transaction_context);
                    lp.execute();

                    m_history_entry.unlink_dists.push_back(p_unlink.long_str());
                    m_history_entry.link_dists.push_back(p_link.long_str());

                    break;
                }
                case SOLVER_TRANSACTION_ERASE:
                {
                    PackageInfo p(s);
                    Console::stream() << "Unlinking " << PackageInfo(s).str();
                    UnlinkPackage up(p, &m_transaction_context);
                    up.execute();
                    m_history_entry.unlink_dists.push_back(p.long_str());
                    break;
                }
                case SOLVER_TRANSACTION_INSTALL:
                {
                    PackageInfo p(s);
                    Console::stream() << "Linking " << p.str();
                    LinkPackage lp(p, fs::path(cache_dir), &m_transaction_context);
                    lp.execute();
                    m_history_entry.link_dists.push_back(p.long_str());

                    JsonLogger::instance().json_down("LINK");
                    JsonLogger::instance().json_append(nlohmann::json(
                        {
                            {"build_number", p.build_number},
                            {"build_string", p.build_string},
                            {"dist_name", p.str()},
                            {"channel", p.channel},
                            {"name", p.name},
                            {"version", p.version}
                        }
                    ));
                    JsonLogger::instance().json_up();

                    break;
                }
                case SOLVER_TRANSACTION_IGNORE:
                    break;
                default:
                    LOG_ERROR << "Exec case not handled: " << ttype;
                    break;
            }
        }

        Console::stream() << "Transaction finished";
        prefix.history().add_entry(m_history_entry);

        // back to the top level if any action was required
        if (!empty())
            JsonLogger::instance().json_up();
        JsonLogger::instance().json_write({{"dry_run", Context::instance().dry_run}, {"prefix", Context::instance().target_prefix}});
        if (empty())
            JsonLogger::instance().json_write({{"message", "All requested packages already installed"}});
        // finally, print the JSON
        if (Context::instance().json) {
            Console::instance().print(JsonLogger::instance().json_log.unflatten().dump(4), true);
        }

        return true;
    }

    auto MTransaction::to_conda() -> to_conda_type
    {
        to_install_type to_install_structured;
        to_remove_type to_remove_structured;
        JsonLogger::instance().json_down("FETCH");

        for (Solvable* s : m_to_remove)
        {
            const char* mediafile = solvable_lookup_str(s, SOLVABLE_MEDIAFILE);
            to_remove_structured.emplace_back(s->repo->name, mediafile);
        }

        for (Solvable* s : m_to_install)
        {
            const char* mediafile = solvable_lookup_str(s, SOLVABLE_MEDIAFILE);
            std::string s_json = solvable_to_json(s).dump(4);
            to_install_structured.emplace_back(s->repo->name, mediafile, s_json);
        }

        to_specs_type specs;
        std::get<0>(specs) = m_history_entry.update;
        std::get<1>(specs) = m_history_entry.remove;

        return std::make_tuple(specs, to_install_structured, to_remove_structured);
    }

    void MTransaction::log_json()
    {
        for (Solvable* s : m_to_install)
            JsonLogger::instance().json_append(solvable_to_json(s));
        JsonLogger::instance().json_up();
    }

    bool MTransaction::fetch_extract_packages(const std::string& cache_dir, std::vector<MRepo*>& repos)
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

            targets.emplace_back(std::make_unique<PackageDownloadExtractTarget>(*mamba_repo, s));
            multi_dl.add(targets[targets.size() - 1]->target(cache_path, m_multi_cache));
        }
        bool downloaded = multi_dl.download(true);

        if (!downloaded)
        {
            LOG_ERROR << "Download didn't finish!";
            return false;
        }
        // make sure that all targets have finished extracting
        while (!Context::instance().sig_interrupt)
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

        return !Context::instance().sig_interrupt && downloaded;
    }

    bool MTransaction::empty()
    {
        return m_to_install.size() == 0 && m_to_remove.size() == 0;
    }

    bool MTransaction::prompt(const std::string& cache_dir, std::vector<MRepo*>& repos)
    {
        if (Context::instance().quiet && Context::instance().always_yes)
        {
            return true;
        }
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

        printers::Table t({"Package", "Version", "Build", "Channel", "Size"});
        t.set_alignment({printers::alignment::left, printers::alignment::right, printers::alignment::left,
                         printers::alignment::left, printers::alignment::right});
        t.set_padding({2, 2, 2, 2, 5});
        Queue classes, pkgs;

        queue_init(&classes);
        queue_init(&pkgs);

        using rows = std::vector<std::vector<printers::FormattedString>>;

        rows downgraded, upgraded, changed, erased, installed, ignored;
        std::size_t total_size = 0;
        auto* pool = m_transaction->pool;

        auto format_row = [this, pool, &total_size](rows& r, Solvable* s, std::size_t flag)
        {
            std::ptrdiff_t dlsize = solvable_lookup_num(s, SOLVABLE_DOWNLOADSIZE, -1);
            printers::FormattedString dlsize_s;
            if (dlsize != -1)
            {
                if (flag & printers::YELLOW)
                {
                    dlsize_s.s = "Ignored";
                }
                else if (this->m_multi_cache.query(s))
                {
                    dlsize_s.s = "Cached";
                    dlsize_s.flag = printers::GREEN;
                }
                else
                {
                    std::stringstream s;
                    to_human_readable_filesize(s, dlsize);
                    dlsize_s.s = s.str();
                    // Hacky hacky
                    if (flag & printers::GREEN) total_size += dlsize;
                }
            }
            printers::FormattedString name;
            name.s = pool_id2str(pool, s->name);
            name.flag = flag;
            const char* build_string = solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR);
            r.push_back({name,
                         printers::FormattedString(pool_id2str(pool, s->evr)),
                         printers::FormattedString(build_string ? build_string : ""),
                         printers::FormattedString(cut_repo_name(s->repo->name)), dlsize_s});
        };

        int mode = SOLVER_TRANSACTION_SHOW_OBSOLETES | SOLVER_TRANSACTION_OBSOLETE_IS_UPGRADE;
        transaction_classify(m_transaction, mode, &classes);

        Id cls;
        for (int i = 0; i < classes.count; i += 4)
        {
            cls = classes.elements[i];
            transaction_classify_pkgs(m_transaction, mode, cls, classes.elements[i + 2],
                                      classes.elements[i + 3], &pkgs);

            for (int j = 0; j < pkgs.count; j++)
            {
                Id p = pkgs.elements[j];
                Solvable *s = m_transaction->pool->solvables + p;

                if (filter(s))
                {
                    format_row(ignored, s, printers::YELLOW);
                    continue;
                }

                switch (cls)
                {
                    case SOLVER_TRANSACTION_UPGRADED:
                        format_row(upgraded, s, printers::RED);
                        format_row(upgraded, m_transaction->pool->solvables + transaction_obs_pkg(m_transaction, p), printers::GREEN);
                        break;
                    case SOLVER_TRANSACTION_CHANGED:
                        format_row(changed, s, printers::RED);
                        format_row(changed, m_transaction->pool->solvables + transaction_obs_pkg(m_transaction, p), printers::GREEN);
                        break;
                    case SOLVER_TRANSACTION_DOWNGRADED:
                        format_row(downgraded, s, printers::RED);
                        format_row(downgraded, m_transaction->pool->solvables + transaction_obs_pkg(m_transaction, p), printers::GREEN);
                        break;
                    case SOLVER_TRANSACTION_ERASE:
                        format_row(erased, s, printers::RED);
                        break;
                    case SOLVER_TRANSACTION_INSTALL:
                        format_row(installed, s, printers::GREEN);
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
            summary << "  Change: " << changed.size() << " packages\n";
        }
        if (upgraded.size())
        {
            t.add_rows("Upgrade:", upgraded);
            summary << "  Upgrade: " << upgraded.size() << " packages\n";
        }
        if (downgraded.size())
        {
            t.add_rows("Downgrade:", downgraded);
            summary << "  Downgrade: " << downgraded.size() << " packages\n";
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
        t.print();
    }
}
