// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_TRANSACTION_HPP
#define MAMBA_CORE_TRANSACTION_HPP

#include <string>
#include <tuple>
#include <vector>

#include "mamba/api/install.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/transaction_context.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/solution.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/package_info.hpp"

namespace mamba
{
    namespace solver
    {
        struct Request;
    }

    class ChannelContext;
    class Context;

    class MTransaction
    {
    public:

        MTransaction(
            const Context& ctx,
            solver::libsolv::Database& db,
            std::vector<specs::PackageInfo> pkgs_to_remove,
            std::vector<specs::PackageInfo> pkgs_to_install,
            MultiPackageCache& caches
        );

        MTransaction(
            const Context& ctx,
            solver::libsolv::Database& db,
            const solver::Request& request,
            solver::Solution solution,
            MultiPackageCache& caches
        );

        // Only use if the packages have been solved previously already.
        MTransaction(
            const Context& ctx,
            solver::libsolv::Database& db,
            std::vector<specs::PackageInfo> packages,
            MultiPackageCache& caches
        );

        MTransaction(const MTransaction&) = delete;
        MTransaction(MTransaction&&) = delete;
        MTransaction& operator=(const MTransaction&) = delete;
        MTransaction& operator=(MTransaction&&) = delete;

        using to_install_type = std::vector<std::tuple<std::string, std::string, std::string>>;
        using to_remove_type = std::vector<std::tuple<std::string, std::string>>;
        using to_specs_type = std::tuple<std::vector<std::string>, std::vector<std::string>>;
        using to_conda_type = std::tuple<to_specs_type, to_install_type, to_remove_type>;

        to_conda_type to_conda();
        void log_json();
        bool fetch_extract_packages(const Context& ctx, ChannelContext& channel_context);
        bool empty();
        bool prompt(const Context& ctx, ChannelContext& channel_context);
        void print(const Context& ctx, ChannelContext& channel_context);
        bool execute(const Context& ctx, ChannelContext& channel_context, PrefixData& prefix);

    private:

        TransactionContext m_transaction_context;
        MultiPackageCache m_multi_cache;
        const fs::u8path m_cache_path;
        solver::Solution m_solution;

        History::UserRequest m_history_entry;

        std::vector<specs::MatchSpec> m_requested_specs;

        MTransaction(const Context& ctx, MultiPackageCache&);
    };

    MTransaction create_explicit_transaction_from_urls(
        const Context& ctx,
        solver::libsolv::Database& db,
        const std::vector<std::string>& urls,
        MultiPackageCache& package_caches,
        std::vector<detail::other_pkg_mgr_spec>& other_specs
    );

    MTransaction create_explicit_transaction_from_lockfile(
        const Context& ctx,
        solver::libsolv::Database& db,
        const fs::u8path& env_lockfile_path,
        const std::vector<std::string>& categories,
        MultiPackageCache& package_caches,
        std::vector<detail::other_pkg_mgr_spec>& other_specs
    );
}  // namespace mamba

#endif  // MAMBA_TRANSACTION_HPP
