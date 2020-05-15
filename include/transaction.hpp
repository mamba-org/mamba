#ifndef MAMBA_TRANSACTION_HPP
#define MAMBA_TRANSACTION_HPP

#include <iomanip>
#include <set>
#include <future>

#include "nlohmann/json.hpp"

#include "thirdparty/filesystem.hpp"

#include "repo.hpp"
#include "fetch.hpp"
#include "package_handling.hpp"
#include "package_cache.hpp"
#include "output.hpp"
#include "prefix_data.hpp"
#include "transaction_context.hpp"

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

        PackageDownloadExtractTarget(const MRepo& repo, Solvable* solvable);

        void write_repodata_record(const fs::path& base_path);
        void add_url();
        bool finalize_callback();
        bool finished();
        bool validate_extract();
        DownloadTarget* target(const fs::path& cache_path, MultiPackageCache& cache);

    private:

        Solvable* m_solv;

        ProgressProxy m_progress_proxy;
        std::unique_ptr<DownloadTarget> m_target;

        std::string m_url, m_name, m_channel, m_filename;
        fs::path m_tarball_path, m_cache_path;

        std::future<bool> m_extract_future;
        bool m_finished;

        static std::mutex extract_mutex;
    };

    class MTransaction
    {
    public:

        MTransaction(MSolver& solver, MultiPackageCache& cache);
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
        bool execute(PrefixData& prefix, const fs::path& cache_dir);
        bool filter(Solvable* s);

        std::string find_python_version();

    private:

        bool m_filter_only_or_ignore;
        std::set<Id> m_filter_name_ids;

        TransactionContext m_transaction_context;
        MultiPackageCache m_multi_cache;
        std::vector<Solvable*> m_to_install, m_to_remove;
        History::UserRequest m_history_entry;
        Transaction* m_transaction;
    };  
}

#endif // MAMBA_TRANSACTION_HPP
