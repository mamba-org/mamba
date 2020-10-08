// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_TRANSACTION_HPP
#define MAMBA_TRANSACTION_HPP

#include <future>
#include <iomanip>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "nlohmann/json.hpp"

#include "fetch.hpp"
#include "mamba_fs.hpp"
#include "output.hpp"
#include "package_cache.hpp"
#include "package_handling.hpp"
#include "prefix_data.hpp"
#include "repo.hpp"
#include "transaction_context.hpp"

extern "C"
{
#include "solv/transaction.h"
}

#include "solver.hpp"


namespace mamba
{
    void try_add(nlohmann::json& j, const char* key, const char* val);
    nlohmann::json solvable_to_json(Solvable* s);

    class PackageDownloadExtractTarget
    {
    public:
        PackageDownloadExtractTarget(Solvable* solvable);
        PackageDownloadExtractTarget(const PackageInfo& pkg_info);

        void write_repodata_record(const fs::path& base_path);
        void add_url();
        bool finalize_callback();
        bool finished();
        bool validate_extract();
        DownloadTarget* target(const fs::path& cache_path, MultiPackageCache& cache);

    private:
        bool m_finished;
        PackageInfo m_package_info;

        std::string m_sha256, m_md5;
        std::size_t m_expected_size;

        ProgressProxy m_progress_proxy;
        std::unique_ptr<DownloadTarget> m_target;

        std::string m_url, m_name, m_channel, m_filename;
        fs::path m_tarball_path, m_cache_path;

        std::future<bool> m_extract_future;

        static std::mutex extract_mutex;
    };

    class MTransaction
    {
    public:
        enum class FilterType
        {
            none,
            keep_only,
            ignore
        };

        MTransaction(MSolver& solver, MultiPackageCache& cache);
        ~MTransaction();

        MTransaction(const MTransaction&) = delete;
        MTransaction& operator=(const MTransaction&) = delete;
        MTransaction(MTransaction&&) = delete;
        MTransaction& operator=(MTransaction&&) = delete;

        using to_install_type = std::vector<std::tuple<std::string, std::string, std::string>>;
        using to_remove_type = std::vector<std::tuple<std::string, std::string>>;
        using to_specs_type = std::tuple<std::vector<std::string>, std::vector<std::string>>;
        using to_conda_type = std::tuple<to_specs_type, to_install_type, to_remove_type>;

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
        FilterType m_filter_type = FilterType::none;
        std::set<Id> m_filter_name_ids;

        TransactionContext m_transaction_context;
        MultiPackageCache m_multi_cache;
        std::vector<Solvable*> m_to_install, m_to_remove;
        History::UserRequest m_history_entry;
        Transaction* m_transaction;

        bool m_force_reinstall = false;
    };
}  // namespace mamba

#endif  // MAMBA_TRANSACTION_HPP
