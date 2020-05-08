#include <thread>

#include "transaction.hpp"
#include "match_spec.hpp"
#include "link.hpp"

namespace mamba
{
    void try_add(nlohmann::json& j, const char* key, const char* val)
    {
        if (!val)
            return;
        j[key] = val;
    }

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

        curl_off_t avg_speed;
        auto cres = curl_easy_getinfo(m_target->handle(), CURLINFO_SPEED_DOWNLOAD_T, &avg_speed);
        if (cres != CURLE_OK)
        {
            avg_speed = 0;
        }

        // Validation
        auto expected_size = solvable_lookup_num(m_solv, SOLVABLE_DOWNLOADSIZE, 0);
        std::string sha256_check = solvable_lookup_checksum(m_solv, SOLVABLE_CHECKSUM, &unused);

        if (m_target->downloaded_size != expected_size)
        {
            throw std::runtime_error("File not valid: file size doesn't match expectation (" + std::string(m_tarball_path) + ")");
        }
        if (!validate::sha256(m_tarball_path, sha256_check))
        {
            throw std::runtime_error("File not valid: SHA256 sum doesn't match expectation (" + std::string(m_tarball_path) + ")");
        }

        m_progress_proxy.set_postfix("Waiting...");

        // Extract path is __not__ yet thread safe it seems...
        {
            std::lock_guard<std::mutex> lock(PackageDownloadExtractTarget::extract_mutex);
            m_progress_proxy.set_postfix("Decompressing...");
            auto extract_path = extract(m_tarball_path);
            write_repodata_record(extract_path);
            add_url();
        }

        std::stringstream final_msg;
        final_msg << "Finished " << std::left << std::setw(30) << m_name << std::right << std::setw(8);
        m_progress_proxy.elapsed_time_to_stream(final_msg);
        final_msg << " " << std::setw(12 + 2);
        to_human_readable_filesize(final_msg, expected_size);
        final_msg << " " << std::setw(6);
        to_human_readable_filesize(final_msg, avg_speed);
        final_msg << "/s";
        m_progress_proxy.mark_as_completed(final_msg.str());

        m_finished = true;
        return m_finished;
    }

    bool PackageDownloadExtractTarget::finalize_callback()
    {
        m_progress_proxy.set_progress(100);
        m_progress_proxy.set_postfix("Validating...");

        m_extract_future = std::async(std::launch::async,
                                      &PackageDownloadExtractTarget::validate_extract,
                                      this);

        return true;
    }

    bool PackageDownloadExtractTarget::finished()
    {
        return m_target == nullptr ? true : m_finished;
    }

    DownloadTarget* PackageDownloadExtractTarget::target(const fs::path& cache_path)
    {
        m_cache_path = cache_path;
        m_tarball_path = cache_path / m_filename;
        bool tarball_exists = fs::exists(m_tarball_path);
        fs::path dest_dir = strip_package_name(m_tarball_path);
        bool dest_dir_exists = fs::exists(dest_dir);
        bool valid = false;

        Id unused;
        if (tarball_exists)
        {
            // validate that this tarball has the right size and MD5 sum
            std::uintmax_t file_size = solvable_lookup_num(m_solv, SOLVABLE_DOWNLOADSIZE, 0);
            valid = validate::file_size(m_tarball_path, file_size);
            valid = valid && validate::md5(m_tarball_path, solvable_lookup_checksum(m_solv, SOLVABLE_PKGID, &unused));
            LOG_INFO << m_tarball_path << " is " << valid;
        }

        if (dest_dir_exists)
        {
            auto repodata_record_path = dest_dir / "info" / "repodata_record.json";
            if (fs::exists(repodata_record_path))
            {
                try
                {
                    std::ifstream repodata_record_f(repodata_record_path);
                    nlohmann::json repodata_record;
                    repodata_record_f >> repodata_record;
                    valid = (repodata_record["size"].get<std::size_t>() == solvable_lookup_num(m_solv, SOLVABLE_DOWNLOADSIZE, 0));
                    valid = valid && repodata_record["sha256"].get<std::string>() == solvable_lookup_checksum(m_solv, SOLVABLE_CHECKSUM, &unused);
                    valid = valid && repodata_record["channel"].get<std::string>() == m_channel;
                    valid = valid && repodata_record["url"].get<std::string>() == m_url;
                    if (!valid)
                    {
                        LOG_WARNING << "Found directory with same name, but different size, channel, url or checksum " << repodata_record_path;
                    }
                }
                catch (...)
                {
                    LOG_WARNING << "Found corrupted repodata_record file " << repodata_record_path;
                }
            }
        }

        if (tarball_exists && !dest_dir_exists)
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

    MTransaction::MTransaction(MSolver& solver)
    {
        if (!solver.is_solved())
        {
            throw std::runtime_error("Cannot create transaction without calling solver.solve() first.");
        }
        m_transaction = solver_create_transaction(solver);

        m_history_entry = History::UserRequest::prefilled();
        m_history_entry.update = solver.install_specs();
        m_history_entry.remove = solver.remove_specs();

        init();
        JsonLogger::instance().json_down("actions");
    }

    MTransaction::~MTransaction()
    {
        LOG_INFO << "Freeing transaction.";
        transaction_free(m_transaction);
        JsonLogger::instance().json_write("PREFIX", Context::instance().target_prefix);
        JsonLogger::instance().json_up();
        JsonLogger::instance().json_write({{"dry_run", Context::instance().dry_run}, {"prefix", Context::instance().target_prefix}});
        if (Context::instance().json)
            Console::instance().print(JsonLogger::instance().json_log.unflatten().dump(4), true);
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
        std::cout << "\n\nTransaction starting\n";
        m_transaction_context = TransactionContext(prefix.path(), find_python_version());
        History::UserRequest ur = History::UserRequest::prefilled();

        transaction_order(m_transaction, 0);

        auto* pool = m_transaction->pool;
        transaction_order(m_transaction, 0);

        for (int i = 0; i < m_transaction->steps.count; i++)
        {
            Id p = m_transaction->steps.elements[i];
            Id ttype = transaction_type(m_transaction, p, SOLVER_TRANSACTION_SHOW_ALL);
            Solvable *s = pool_id2solvable(pool, p);
            switch (ttype)
            {
                case SOLVER_TRANSACTION_DOWNGRADED:
                case SOLVER_TRANSACTION_UPGRADED:
                case SOLVER_TRANSACTION_CHANGED:
                {
                    Solvable* s2 = m_transaction->pool->solvables + transaction_obs_pkg(m_transaction, p);
                    std::cout << "Changing " << PackageInfo(s).str() << " ==> " << PackageInfo(s2).str() << "\n";
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
                    std::cout << "Unlinking " << PackageInfo(s).str() << "\n";
                    UnlinkPackage up(p, &m_transaction_context);
                    up.execute();
                    m_history_entry.unlink_dists.push_back(p.long_str());
                    break;
                }
                case SOLVER_TRANSACTION_INSTALL:
                {
                    PackageInfo p(s);
                    std::cout << "Linking " << PackageInfo(s).str() << "\n";
                    LinkPackage lp(p, fs::path(cache_dir), &m_transaction_context);
                    lp.execute();
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

        std::cout << "Transaction finished" << std::endl;
        prefix.history().add_entry(m_history_entry);

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

        return std::make_tuple(to_install_structured, to_remove_structured);
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
            multi_dl.add(targets[targets.size() - 1]->target(cache_path));
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
        // check size of transaction
        Console::print("\n");
        if (empty())
        {
            Console::print("# All requested packages already installed\n");
            return true;
        }

        // we print, even if quiet
        print();
        if (Context::instance().dry_run)
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
        transaction_print(m_transaction);
    }
}
