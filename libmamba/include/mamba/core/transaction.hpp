// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_TRANSACTION_HPP
#define MAMBA_CORE_TRANSACTION_HPP

#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "mamba/api/install.hpp"

#include "mamba_fs.hpp"
#include "match_spec.hpp"
#include "package_cache.hpp"
#include "package_info.hpp"
#include "prefix_data.hpp"
#include "solver.hpp"
#include "transaction_context.hpp"

namespace mamba::solv
{
    class ObjTransaction;
    class ObjSolvableViewConst;
}

namespace mamba
{
    class ChannelContext;

    class MTransaction
    {
    public:

        enum class FilterType
        {
            none,
            keep_only,
            ignore
        };

        MTransaction(
            MPool& pool,
            const std::vector<MatchSpec>& specs_to_remove,
            const std::vector<MatchSpec>& specs_to_install,
            MultiPackageCache& caches
        );
        MTransaction(MPool& pool, MSolver& solver, MultiPackageCache& caches);

        // Only use if the packages have been solved previously already.
        MTransaction(MPool& pool, const std::vector<PackageInfo>& packages, MultiPackageCache& caches);

        ~MTransaction();

        MTransaction(const MTransaction&) = delete;
        MTransaction& operator=(const MTransaction&) = delete;
        MTransaction(MTransaction&&) = delete;
        MTransaction& operator=(MTransaction&&) = delete;

        using to_install_type = std::vector<std::tuple<std::string, std::string, std::string>>;
        using to_remove_type = std::vector<std::tuple<std::string, std::string>>;
        using to_specs_type = std::tuple<std::vector<std::string>, std::vector<std::string>>;
        using to_conda_type = std::tuple<to_specs_type, to_install_type, to_remove_type>;

        to_conda_type to_conda();
        void log_json();
        bool fetch_extract_packages();
        bool empty();
        bool prompt();
        void print();
        bool execute(PrefixData& prefix);

        std::pair<std::string, std::string> find_python_version();

    private:

        MPool m_pool;

        FilterType m_filter_type = FilterType::none;
        std::set<Id> m_filter_name_ids;

        TransactionContext m_transaction_context;
        MultiPackageCache m_multi_cache;
        const fs::u8path m_cache_path;
        std::vector<PackageInfo> m_to_install;
        std::vector<PackageInfo> m_to_remove;

        History::UserRequest m_history_entry = History::UserRequest::prefilled();
        // Temporarily using Pimpl for encapsulation
        std::unique_ptr<solv::ObjTransaction> m_transaction;

        std::vector<MatchSpec> m_requested_specs;

        void init();
        bool filter(const solv::ObjSolvableViewConst& s) const;

        auto trans() -> solv::ObjTransaction&;
        auto trans() const -> const solv::ObjTransaction&;
    };

    MTransaction create_explicit_transaction_from_urls(
        MPool& pool,
        const std::vector<std::string>& urls,
        MultiPackageCache& package_caches,
        std::vector<detail::other_pkg_mgr_spec>& other_specs
    );

    MTransaction create_explicit_transaction_from_lockfile(
        MPool& pool,
        const fs::u8path& env_lockfile_path,
        const std::vector<std::string>& categories,
        MultiPackageCache& package_caches,
        std::vector<detail::other_pkg_mgr_spec>& other_specs
    );
}  // namespace mamba

#endif  // MAMBA_TRANSACTION_HPP
