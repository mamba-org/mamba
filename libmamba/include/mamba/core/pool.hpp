// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_POOL_HPP
#define MAMBA_CORE_POOL_HPP

#include <functional>
#include <memory>
#include <optional>

#include "mamba/core/error_handling.hpp"
#include "mamba/solver/libsolv/parameters.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/loop_control.hpp"

namespace mamba
{
    class Context;
    class PrefixData;
    class SubdirData;

    namespace fs
    {
        class u8path;
    }

    namespace solv
    {
        class ObjPool;
    }

    namespace specs
    {
        class MatchSpec;
    }

    namespace solver::libsolv
    {
        class Solver;
        class UnSolvable;
    }

    /**
     * Pool of solvable involved in resolving en environment.
     *
     * The pool contains the solvable (packages) information required from the ``MSolver``.
     * The pool can be reused by multiple solvers to solve differents requirements with the same
     * ecosystem.
     * Behaves like a ``std::shared_ptr``, meaning ressources are shared on copy.
     */
    class Database
    {
    public:

        using logger_type = std::function<void(solver::libsolv::LogLevel, std::string_view)>;

        Database(specs::ChannelResolveParams channel_params);
        Database(const Database&) = delete;
        Database(Database&&);

        ~Database();

        auto operator=(const Database&) -> Database& = delete;
        auto operator=(Database&&) -> Database&;

        [[nodiscard]] auto channel_params() const -> const specs::ChannelResolveParams&;

        void set_logger(logger_type callback);

        auto add_repo_from_repodata_json(
            const fs::u8path& path,
            std::string_view url,
            solver::libsolv::PipAsPythonDependency add = solver::libsolv::PipAsPythonDependency::No,
            solver::libsolv::UseOnlyTarBz2 only_tar = solver::libsolv::UseOnlyTarBz2::No,
            solver::libsolv::RepodataParser parser = solver::libsolv::RepodataParser::Mamba
        ) -> expected_t<solver::libsolv::RepoInfo>;

        auto add_repo_from_native_serialization(
            const fs::u8path& path,
            const solver::libsolv::RepodataOrigin& expected,
            solver::libsolv::PipAsPythonDependency add = solver::libsolv::PipAsPythonDependency::No
        ) -> expected_t<solver::libsolv::RepoInfo>;

        template <typename Iter>
        auto add_repo_from_packages(
            Iter first_package,
            Iter last_package,
            std::string_view name = "",
            solver::libsolv::PipAsPythonDependency add = solver::libsolv::PipAsPythonDependency::No
        ) -> solver::libsolv::RepoInfo;

        template <typename Range>
        auto add_repo_from_packages(
            const Range& packages,
            std::string_view name = "",
            solver::libsolv::PipAsPythonDependency add = solver::libsolv::PipAsPythonDependency::No
        ) -> solver::libsolv::RepoInfo;

        auto native_serialize_repo(
            const solver::libsolv::RepoInfo& repo,
            const fs::u8path& path,
            const solver::libsolv::RepodataOrigin& metadata
        ) -> expected_t<solver::libsolv::RepoInfo>;

        [[nodiscard]] auto installed_repo() const -> std::optional<solver::libsolv::RepoInfo>;

        void set_installed_repo(solver::libsolv::RepoInfo repo);

        void
        set_repo_priority(solver::libsolv::RepoInfo repo, solver::libsolv::Priorities priorities);

        void remove_repo(solver::libsolv::RepoInfo repo);

        template <typename Func>
        void for_each_package_in_repo(solver::libsolv::RepoInfo repo, Func&&) const;

        template <typename Func>
        void for_each_package_matching(const specs::MatchSpec& ms, Func&&);

        template <typename Func>
        void for_each_package_depending_on(const specs::MatchSpec& ms, Func&&);

        /**
         * An access control wrapper.
         *
         * This struct is to control who can access the underlying private representation
         * of the ObjPool without giving them full friend access.
         */
        class Impl
        {
            [[nodiscard]] static auto get(Database& pool) -> solv::ObjPool&;
            [[nodiscard]] static auto get(const Database& pool) -> const solv::ObjPool&;

            friend class solver::libsolv::Solver;
            friend class solver::libsolv::UnSolvable;
        };

    private:

        struct DatabaseImpl;

        std::unique_ptr<DatabaseImpl> m_data;

        friend class Impl;
        auto pool() -> solv::ObjPool&;
        [[nodiscard]] auto pool() const -> const solv::ObjPool&;

        auto add_repo_from_packages_impl_pre(std::string_view name) -> solver::libsolv::RepoInfo;
        void add_repo_from_packages_impl_loop(
            const solver::libsolv::RepoInfo& repo,
            const specs::PackageInfo& pkg
        );
        void add_repo_from_packages_impl_post(
            const solver::libsolv::RepoInfo& repo,
            solver::libsolv::PipAsPythonDependency add
        );

        enum class PackageId : int;

        [[nodiscard]] auto package_id_to_package_info(PackageId id) const -> specs::PackageInfo;

        [[nodiscard]] auto packages_in_repo(solver::libsolv::RepoInfo repo) const
            -> std::vector<PackageId>;

        [[nodiscard]] auto packages_matching_ids(const specs::MatchSpec& ms)
            -> std::vector<PackageId>;

        [[nodiscard]] auto packages_depending_on_ids(const specs::MatchSpec& ms)
            -> std::vector<PackageId>;
    };

    // TODO machinery functions in separate files
    void add_spdlog_logger_to_pool(Database& pool);

    auto load_subdir_in_pool(const Context& ctx, Database& pool, const SubdirData& subdir)
        -> expected_t<solver::libsolv::RepoInfo>;

    auto load_installed_packages_in_pool(const Context& ctx, Database& pool, const PrefixData& prefix)
        -> solver::libsolv::RepoInfo;

    /********************
     *  Implementation  *
     ********************/

    template <typename Iter>
    auto Database::add_repo_from_packages(
        Iter first_package,
        Iter last_package,
        std::string_view name,
        solver::libsolv::PipAsPythonDependency add
    ) -> solver::libsolv::RepoInfo
    {
        auto repo = add_repo_from_packages_impl_pre(name);
        for (; first_package != last_package; ++first_package)
        {
            add_repo_from_packages_impl_loop(repo, *first_package);
        }
        add_repo_from_packages_impl_post(repo, add);
        return repo;
    }

    template <typename Range>
    auto Database::add_repo_from_packages(
        const Range& packages,
        std::string_view name,
        solver::libsolv::PipAsPythonDependency add
    ) -> solver::libsolv::RepoInfo
    {
        return add_repo_from_packages(packages.begin(), packages.end(), name, add);
    }

    // TODO(C++20): Use ranges::transform
    template <typename Func>
    void Database::for_each_package_in_repo(solver::libsolv::RepoInfo repo, Func&& func) const
    {
        for (auto id : packages_in_repo(repo))
        {
            if constexpr (std::is_same_v<decltype(func(package_id_to_package_info(id))), util::LoopControl>)
            {
                if (func(package_id_to_package_info(id)) == util::LoopControl::Break)
                {
                    break;
                }
            }
            else
            {
                func(package_id_to_package_info(id));
            }
        }
    }

    // TODO(C++20): Use ranges::transform
    template <typename Func>
    void Database::for_each_package_matching(const specs::MatchSpec& ms, Func&& func)
    {
        for (auto id : packages_matching_ids(ms))
        {
            if constexpr (std::is_same_v<decltype(func(package_id_to_package_info(id))), util::LoopControl>)
            {
                if (func(package_id_to_package_info(id)) == util::LoopControl::Break)
                {
                    break;
                }
            }
            else
            {
                func(package_id_to_package_info(id));
            }
        }
    }

    // TODO(C++20): Use ranges::transform
    template <typename Func>
    void Database::for_each_package_depending_on(const specs::MatchSpec& ms, Func&& func)
    {
        for (auto id : packages_depending_on_ids(ms))
        {
            if constexpr (std::is_same_v<decltype(func(package_id_to_package_info(id))), util::LoopControl>)
            {
                if (func(package_id_to_package_info(id)) == util::LoopControl::Break)
                {
                    break;
                }
            }
            else
            {
                func(package_id_to_package_info(id));
            }
        }
    }
}
#endif
