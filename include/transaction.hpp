#ifndef MAMBA_TRANSACTION_HPP
#define MAMBA_TRANSACTION_HPP

#include <iomanip>
#include "nlohmann/json.hpp"

#include "thirdparty/filesystem.hpp"

#include "repo.hpp"
#include "fetch.hpp"
#include "package_handling.hpp"
#include "output.hpp"

extern "C"
{
    #include "solv/transaction.h"
}

#include "solver.hpp"

namespace fs = ghc::filesystem;

namespace mamba
{
    void try_add(nlohmann::json& j, const char* key, const char* val);
    nlohmann::json solvable_to_json(Solvable* s);

    class PackageDownloadExtractTarget
    {
    public:

        PackageDownloadExtractTarget(MRepo* repo, Solvable* solvable);

        void write_repodata_record(const fs::path& base_path);
        void add_url();
        int finalize_callback();
        std::unique_ptr<DownloadTarget>& target(const fs::path& cache_path);

    private:

        MRepo* m_repo;
        Solvable* m_solv;

        ProgressProxy m_progress_proxy;
        std::unique_ptr<DownloadTarget> m_target;

        std::string m_url, m_name, m_channel, m_filename;
        fs::path m_tarball_path, m_cache_path;
    };

    class MTransaction
    {
    public:

        MTransaction(MSolver& solver);
        ~MTransaction();

        MTransaction(const MTransaction&) = delete;
        MTransaction& operator=(const MTransaction&) = delete;
        MTransaction(MTransaction&&) = delete;
        MTransaction& operator=(MTransaction&&) = delete;

        using to_install_type = std::vector<std::tuple<std::string, std::string, std::string>>;
        using to_remove_type = std::vector<std::tuple<std::string, std::string>>;
        using to_conda_type = std::tuple<to_install_type, to_remove_type>;

        void init();
        to_conda_type to_conda();
        void log_json();
        bool fetch_extract_packages(const std::string& cache_dir, std::vector<MRepo*>& repos);
        bool empty();
        bool prompt(const std::string& cache_dir, std::vector<MRepo*>& repos);
        void print();

    private:

        std::vector<Solvable*> m_to_install, m_to_remove;
        Transaction* m_transaction;
    };  
}

#endif // MAMBA_TRANSACTION_HPP
