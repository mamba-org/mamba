#pragma once

#include <iomanip>
#include "nlohmann/json.hpp"

#include "thirdparty/minilog.hpp"
#include "thirdparty/filesystem.hpp"

#include "repo.hpp"
#include "fetch.hpp"
#include "package_handling.hpp"

extern "C"
{
    #include "solv/transaction.h"
}

#include "solver.hpp"

namespace fs = ghc::filesystem;

namespace mamba
{
    inline void try_add(nlohmann::json& j, const char* key, const char* val)
    {
        if (!val)
            return;
        j[key] = val;
    }

    nlohmann::json solvable_to_json(Solvable* s)
    {
        nlohmann::json j;
        auto* pool = s->repo->pool;

        j["name"] = pool_id2str(pool, s->name);
        j["version"] = pool_id2str(pool, s->evr);

        try_add(j, "build", solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR));
        // Note we should?! fix this in libsolv?!
        const char* build_number = solvable_lookup_str(s, SOLVABLE_BUILDVERSION);
        if (build_number)
        {
            j["build_number"] = std::stoi(build_number);
        }

        try_add(j, "license", solvable_lookup_str(s, SOLVABLE_LICENSE));
        j["size"] = solvable_lookup_num(s, SOLVABLE_DOWNLOADSIZE, -1);

        std::size_t timestamp = solvable_lookup_num(s, SOLVABLE_BUILDTIME, 0);
        if (timestamp != 0) timestamp *= 1000;
        j["timestamp"] = timestamp;

        Id check_type;
        try_add(j, "md5", solvable_lookup_checksum(s, SOLVABLE_PKGID, &check_type));
        try_add(j, "sha256", solvable_lookup_checksum(s, SOLVABLE_CHECKSUM, &check_type));

        j["subdir"] = solvable_lookup_str(s, SOLVABLE_MEDIADIR);
        j["fn"] = solvable_lookup_str(s, SOLVABLE_MEDIAFILE);

        std::vector<std::string> depends, constrains;
        Queue q;
        queue_init(&q);
        solvable_lookup_deparray(s, SOLVABLE_REQUIRES, &q, -1);
        depends.resize(q.count);
        for (int i = 0; i < q.count; ++i)
        {
            depends[i] = pool_dep2str(pool, q.elements[i]);
        }
        queue_empty(&q);
        solvable_lookup_deparray(s, SOLVABLE_CONSTRAINS, &q, -1);
        constrains.resize(q.count);
        for (int i = 0; i < q.count; ++i)
        {
            constrains[i] = pool_dep2str(pool, q.elements[i]);
        }
        queue_free(&q);

        j["depends"] = depends;
        j["constrains"] = constrains;

        return j;
    }

    class PackageDownloadExtractTarget
    {
    public:
        PackageDownloadExtractTarget(MRepo* repo, Solvable* solvable)
            : m_repo(repo), m_solv(solvable)
        {
        }

        void write_repodata_record(const fs::path& base_path)
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

        void add_url()
        {
            std::ofstream urls_txt(m_cache_path / "urls.txt", std::ios::app);
            urls_txt << m_url << std::endl;
        }

        int finalize_callback()
        {
            Id __unused__;

            m_progress_proxy.set_option(indicators::option::PostfixText{"Validating..."});

            // Validation
            auto expected_size = solvable_lookup_num(m_solv, SOLVABLE_DOWNLOADSIZE, 0);
            std::string sha256_check = solvable_lookup_checksum(m_solv, SOLVABLE_CHECKSUM, &__unused__);

            if (m_target->downloaded_size != expected_size)
            {
                throw std::runtime_error("File not valid: file size doesn't match expectation (" + std::string(m_tarball_path) + ")");
            }
            if (!validate::sha256(m_tarball_path, expected_size, sha256_check))
            {
                throw std::runtime_error("File not valid: SHA256 sum doesn't match expectation (" + std::string(m_tarball_path) + ")");
            }

            m_progress_proxy.set_option(indicators::option::PostfixText{"Decompressing..."});
            auto extract_path = extract(m_tarball_path);
            write_repodata_record(extract_path);
            add_url();
            m_progress_proxy.set_option(indicators::option::PostfixText{"Done"});
            m_progress_proxy.mark_as_completed("Downloaded & extracted " + m_name);
            return 0;
        }

        std::unique_ptr<DownloadTarget>& target(const fs::path& cache_path)
        {
            m_filename = solvable_lookup_str(m_solv, SOLVABLE_MEDIAFILE);
            m_cache_path = cache_path;
            m_tarball_path = cache_path / m_filename;
            bool tarball_exists = fs::exists(m_tarball_path);

            bool valid = false;

            Id __unused__;
            if (tarball_exists)
            {
                // validate that this tarball has the right size and MD5 sum
                std::uintmax_t file_size = solvable_lookup_num(m_solv, SOLVABLE_DOWNLOADSIZE, 0);
                valid = validate::file_size(m_tarball_path, file_size);
                valid = valid && validate::md5(m_tarball_path, file_size, solvable_lookup_checksum(m_solv, SOLVABLE_PKGID, &__unused__));
                LOG(INFO) << m_tarball_path << " is " << valid;
            }

            if (!tarball_exists || !valid)
            {
                // need to download this file
                m_channel = m_repo->url();
                m_url = m_channel + "/" + m_filename;
                m_name = pool_id2str(m_solv->repo->pool, m_solv->name);

                LOG(INFO) << "Adding " << m_name << " with " << m_url;

                m_progress_proxy = Output::instance().add_progress_bar(m_name);
                m_target = std::make_unique<DownloadTarget>(m_name, m_url, cache_path / m_filename);
                m_target->set_finalize_callback(&PackageDownloadExtractTarget::finalize_callback, this);
                m_target->set_expected_size(solvable_lookup_num(m_solv, SOLVABLE_DOWNLOADSIZE, 0));
                m_target->set_progress_bar(m_progress_proxy);
            }
            return m_target;
        }

        MRepo* m_repo;
        Solvable* m_solv;

        Output::ProgressProxy m_progress_proxy;
        std::unique_ptr<DownloadTarget> m_target;

        std::string m_url, m_name, m_channel, m_filename;
        fs::path m_tarball_path, m_cache_path;
    };

    class MTransaction
    {
    public:
        MTransaction(MSolver& solver)
        {
            if (!solver.is_solved())
            {
                throw std::runtime_error("Cannot create transaction without calling solver.solve() first.");
            }
            m_transaction = solver_create_transaction(solver);
            init();
        }

        ~MTransaction()
        {
            LOG(INFO) << "Freeing transaction.";
            transaction_free(m_transaction);
        }

        void init()
        {
            Queue classes, pkgs;

            queue_init(&classes);
            queue_init(&pkgs);

            int mode = SOLVER_TRANSACTION_SHOW_OBSOLETES |
                       SOLVER_TRANSACTION_OBSOLETE_IS_UPGRADE;

            transaction_classify(m_transaction, mode, &classes);

            Id cls;
            std::string location;
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
                        case SOLVER_TRANSACTION_VENDORCHANGE:
                        case SOLVER_TRANSACTION_ARCHCHANGE:
                        default:
                            LOG(WARNING) << "CASE NOT HANDLED. " << cls;
                            break;
                    }
                }
            }

            queue_free(&classes);
            queue_free(&pkgs);
        }

        auto to_conda()
        {
            std::vector<std::tuple<std::string, std::string, std::string>> to_install_structured; 
            std::vector<std::tuple<std::string, std::string>> to_remove_structured; 

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

        auto fetch_extract_packages(const std::string& cache_dir, std::vector<MRepo*>& repos)
        {
            fs::path cache_path(cache_dir);
            std::vector<std::unique_ptr<PackageDownloadExtractTarget>> targets;
            MultiDownloadTarget multi_dl;

            Output::instance().init_multi_progress();

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

                auto dl_target = std::make_unique<PackageDownloadExtractTarget>(mamba_repo, s);
                multi_dl.add(dl_target->target(cache_path));
                targets.push_back(std::move(dl_target));
            }
            multi_dl.download(true);
        }

        bool empty()
        {
            return m_to_install.size() == 0 && m_to_remove.size() == 0;
        }

        bool prompt(const std::string& cache_dir, std::vector<MRepo*>& repos)
        {
            if (Context::instance().quiet && Context::instance().always_yes)
            {
                return true;
            }
            // check size of transaction
            Output::print("\n");
            if (empty())
            {
                Output::print("# All requested packages already installed\n");
                return true;
            }

            // we print, even if quiet
            print();
            if (Context::instance().dry_run)
            {
                return true;
            }
            bool res = Output::prompt("Confirm changes", 'y');
            if (res)
            {
                fetch_extract_packages(cache_dir, repos);
            }
            return true;
        }

        void print()
        {
            transaction_print(m_transaction);
        }

    private:
        std::vector<Solvable*> m_to_install, m_to_remove;
        Transaction* m_transaction;
    };  
}