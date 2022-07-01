// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_TRANSACTION_HPP
#define MAMBA_CORE_TRANSACTION_HPP

#include <future>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "nlohmann/json.hpp"

#include "fetch.hpp"
#include "mamba_fs.hpp"
#include "match_spec.hpp"
#include "package_cache.hpp"
#include "package_handling.hpp"
#include "prefix_data.hpp"
#include "progress_bar.hpp"
#include "repo.hpp"
#include "thread_utils.hpp"
#include "transaction_context.hpp"
#include "env_lockfile.hpp"
#include "mamba/api/install.hpp"

#include <powerloader/download_target.hpp>

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

        void write_repodata_record(const fs::u8path& base_path);
        void add_url();
        bool finalize_callback();
        bool finished();
        void validate();
        bool extract();
        bool extract_from_cache();
        bool validate_extract();
        const std::string& name() const;
        std::size_t expected_size() const;
        auto validation_result() const;
        void clear_cache() const;

        std::shared_ptr<powerloader::DownloadTarget> target(MultiPackageCache& cache);

        enum VALIDATION_RESULT
        {
            UNDEFINED = 0,
            VALID = 1,
            SHA256_ERROR,
            MD5SUM_ERROR,
            SIZE_ERROR,
            EXTRACT_ERROR
        };

        std::exception m_decompress_exception;

    private:
        bool m_finished;
        PackageInfo m_package_info;

        std::string m_sha256, m_md5;
        std::size_t m_expected_size;

        bool m_has_progress_bars = false;
        ProgressProxy m_download_bar, m_extract_bar;
        std::shared_ptr<powerloader::DownloadTarget> m_target;

        std::string m_url, m_name, m_filename;
        fs::u8path m_tarball_path, m_cache_path;

        std::future<bool> m_extract_future;

        VALIDATION_RESULT m_validation_result = VALIDATION_RESULT::UNDEFINED;

        std::function<void(ProgressBarRepr&)> extract_repr();
        std::function<void(ProgressProxy&)> extract_progress_callback();
    };

    class DownloadExtractSemaphore
    {
    public:
        static std::ptrdiff_t get_max();
        static void set_max(int value);

    private:
        static counting_semaphore semaphore;

        friend class PackageDownloadExtractTarget;
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

        MTransaction(MPool& pool,
                     const std::vector<MatchSpec>& specs_to_remove,
                     const std::vector<MatchSpec>& specs_to_install,
                     MultiPackageCache& caches);
        MTransaction(MSolver& solver, MultiPackageCache& caches);

        // Only use if the packages have been solved previously already.
        MTransaction(MPool& pool,
                     const std::vector<PackageInfo>& packages,
                     MultiPackageCache& caches);

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
        bool fetch_extract_packages();
        bool empty();
        bool prompt();
        void print();
        bool execute(PrefixData& prefix);
        bool filter(Solvable* s);

        std::pair<std::string, std::string> find_python_version();

    private:
        FilterType m_filter_type = FilterType::none;
        std::set<Id> m_filter_name_ids;

        TransactionContext m_transaction_context;
        MultiPackageCache m_multi_cache;
        const fs::u8path m_cache_path;
        std::vector<Solvable*> m_to_install, m_to_remove;

        History::UserRequest m_history_entry;
        Transaction* m_transaction;

        std::vector<MatchSpec> m_requested_specs;

        bool m_force_reinstall = false;
    };

    MTransaction create_explicit_transaction_from_urls(
        MPool& pool,
        const std::vector<std::string>& urls,
        MultiPackageCache& package_caches,
        std::vector<detail::other_pkg_mgr_spec>& other_specs);

    MTransaction create_explicit_transaction_from_lockfile(
        MPool& pool,
        const fs::u8path& env_lockfile_path,
        const std::vector<std::string>& categories,
        MultiPackageCache& package_caches,
        std::vector<detail::other_pkg_mgr_spec>& other_specs);
}  // namespace mamba

#endif  // MAMBA_TRANSACTION_HPP
