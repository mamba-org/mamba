// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLVER_LIBSOLV_DATABASE_HPP
#define MAMBA_SOLVER_LIBSOLV_DATABASE_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string_view>

#include "mamba/core/error_handling.hpp"
#include "mamba/solver/libsolv/parameters.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/loop_control.hpp"

namespace solv
{
    class ObjPool;
}

namespace mamba
{
    namespace fs
    {
        class u8path;
    }

    namespace specs
    {
        class MatchSpec;
    }
}

namespace mamba::solver::libsolv
{
    class Solver;
    class UnSolvable;

    /**
     * Database of solvable involved in resolving en environment.
     *
     * The database contains the solvable (packages) information required from the @ref Solver.
     * The database can be reused by multiple solvers to solve different requirements with the
     * same ecosystem.
     */
    class Database
    {
    public:

        /**
         * Global Database settings.
         */
        struct Settings
        {
            MatchSpecParser matchspec_parser = MatchSpecParser::Libsolv;
        };

        using logger_type = std::function<void(LogLevel, std::string_view)>;

        explicit Database(specs::ChannelResolveParams channel_params);
        Database(specs::ChannelResolveParams channel_params, Settings settings);
        Database(const Database&) = delete;
        Database(Database&&);

        ~Database();

        auto operator=(const Database&) -> Database& = delete;
        auto operator=(Database&&) -> Database&;

        [[nodiscard]] auto channel_params() const -> const specs::ChannelResolveParams&;

        [[nodiscard]] auto settings() const -> const Settings&;

        void set_logger(logger_type callback);

        auto add_repo_from_repodata_json(
            const fs::u8path& path,
            std::string_view url,
            const std::string& channel_id,
            PipAsPythonDependency add = PipAsPythonDependency::No,
            PackageTypes package_types = PackageTypes::CondaOrElseTarBz2,
            VerifyPackages verify_packages = VerifyPackages::No,
            RepodataParser repo_parser = RepodataParser::Mamba
        ) -> expected_t<RepoInfo>;

        auto add_repo_from_native_serialization(
            const fs::u8path& path,
            const RepodataOrigin& expected,
            const std::string& channel_id,
            PipAsPythonDependency add = PipAsPythonDependency::No
        ) -> expected_t<RepoInfo>;

        template <typename Iter>
        auto add_repo_from_packages(
            Iter first_package,
            Iter last_package,
            std::string_view name = "",
            PipAsPythonDependency add = PipAsPythonDependency::No
        ) -> RepoInfo;

        template <typename Range>
        auto add_repo_from_packages(
            const Range& packages,
            std::string_view name = "",
            PipAsPythonDependency add = PipAsPythonDependency::No
        ) -> RepoInfo;

        auto
        native_serialize_repo(const RepoInfo& repo, const fs::u8path& path, const RepodataOrigin& metadata)
            -> expected_t<RepoInfo>;

        [[nodiscard]] auto installed_repo() const -> std::optional<RepoInfo>;

        void set_installed_repo(RepoInfo repo);

        void set_repo_priority(RepoInfo repo, Priorities priorities);

        void remove_repo(RepoInfo repo);

        [[nodiscard]] auto repo_count() const -> std::size_t;

        [[nodiscard]] auto package_count() const -> std::size_t;

        template <typename Func>
        void for_each_package_in_repo(RepoInfo repo, Func&&) const;

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
            [[nodiscard]] static auto get(Database& database) -> solv::ObjPool&;
            [[nodiscard]] static auto get(const Database& database) -> const solv::ObjPool&;

            friend class Solver;
            friend class UnSolvable;
        };

    private:

        struct DatabaseImpl;

        std::unique_ptr<DatabaseImpl> m_data;

        friend class Impl;
        auto pool() -> solv::ObjPool&;
        [[nodiscard]] auto pool() const -> const solv::ObjPool&;

        auto add_repo_from_packages_impl_pre(std::string_view name) -> RepoInfo;
        void add_repo_from_packages_impl_loop(const RepoInfo& repo, const specs::PackageInfo& pkg);
        void add_repo_from_packages_impl_post(const RepoInfo& repo, PipAsPythonDependency add);

        enum class PackageId : int;

        [[nodiscard]] auto package_id_to_package_info(PackageId id) const -> specs::PackageInfo;

        [[nodiscard]] auto packages_in_repo(RepoInfo repo) const -> std::vector<PackageId>;

        [[nodiscard]] auto packages_matching_ids(const specs::MatchSpec& ms)
            -> std::vector<PackageId>;

        [[nodiscard]] auto packages_depending_on_ids(const specs::MatchSpec& ms)
            -> std::vector<PackageId>;
    };

    /********************
     *  Implementation  *
     ********************/

    template <typename Iter>
    auto Database::add_repo_from_packages(
        Iter first_package,
        Iter last_package,
        std::string_view name,
        PipAsPythonDependency add
    ) -> RepoInfo
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
    auto
    Database::add_repo_from_packages(const Range& packages, std::string_view name, PipAsPythonDependency add)
        -> RepoInfo
    {
        return add_repo_from_packages(packages.begin(), packages.end(), name, add);
    }

    // TODO(C++20): Use ranges::transform
    template <typename Func>
    void Database::for_each_package_in_repo(RepoInfo repo, Func&& func) const
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
