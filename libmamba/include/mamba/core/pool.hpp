// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_POOL_HPP
#define MAMBA_CORE_POOL_HPP

#include <memory>
#include <optional>

#include <solv/pooltypes.h>

#include "mamba/core/error_handling.hpp"
#include "mamba/solver/libsolv/parameters.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/specs/package_info.hpp"

namespace mamba
{
    class ChannelContext;
    class Context;
    class PrefixData;
    class MSubdirData;

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

    /**
     * Pool of solvable involved in resolving en environment.
     *
     * The pool contains the solvable (packages) information required from the ``MSolver``.
     * The pool can be reused by multiple solvers to solve differents requirements with the same
     * ecosystem.
     * Behaves like a ``std::shared_ptr``, meaning ressources are shared on copy.
     */
    class MPool
    {
    public:

        MPool(Context& ctx, ChannelContext& channel_context);
        ~MPool();

        void set_debuglevel();
        void create_whatprovides();

        std::vector<Id> select_solvables(Id id, bool sorted = false) const;
        Id matchspec2id(const specs::MatchSpec& ms);

        std::optional<specs::PackageInfo> id2pkginfo(Id solv_id) const;
        std::optional<std::string> dep2str(Id dep_id) const;

        // TODO: (TMP) This is not meant to exist but is needed for a transition period
        operator ::Pool*();
        operator const ::Pool*() const;

        // TODO: (TMP) This is not meant to be public but is needed for a transition period
        solv::ObjPool& pool();
        const solv::ObjPool& pool() const;

        void remove_repo(::Id repo_id, bool reuse_ids);

        auto add_repo_from_repodata_json(
            const fs::u8path& path,
            std::string_view url,
            solver::libsolv::PipAsPythonDependency add = solver::libsolv::PipAsPythonDependency::No,
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

        void set_installed_repo(const solver::libsolv::RepoInfo& repo);

        void set_repo_priority(const solver::libsolv::RepoInfo& repo, solver::libsolv::Priorities);

        ChannelContext& channel_context() const;
        const Context& context() const;

    private:

        struct MPoolData;


        /**
         * Make MPool behave like a shared_ptr (with move and copy).
         *
         * The pool is passed to the ``MSolver`` for its lifetime but the pool can legitimely
         * be reused with different solvers (in fact it is in ``conda-forge``'s ``regro-bot``
         * and ``boa``).
         * An alternative considered was to make ``MPool`` a move-only type, moving it in and out
         * of ``MSolver`` (effectively borrowing).
         * It was decided to make ``MPool`` share ressources for the following reasons.
         *    - Rvalue semantics would be unexpected in Python (and breaking ``conda-forge``);
         *    - Facilitate (potential) future investigation of parallel solves.
         */
        std::shared_ptr<MPoolData> m_data;

        auto add_repo_from_packages_impl_pre(std::string_view name) -> solver::libsolv::RepoInfo;
        void add_repo_from_packages_impl_loop(
            const solver::libsolv::RepoInfo& repo,
            const specs::PackageInfo& pkg
        );
        void add_repo_from_packages_impl_post(
            const solver::libsolv::RepoInfo& repo,
            solver::libsolv::PipAsPythonDependency add
        );
    };

    // TODO machinery functions in separate files
    auto load_subdir_in_pool(const Context& ctx, MPool& pool, const MSubdirData& subdir)
        -> expected_t<solver::libsolv::RepoInfo>;

    auto load_installed_packages_in_pool(const Context& ctx, MPool& pool, const PrefixData& prefix)
        -> solver::libsolv::RepoInfo;

    /********************
     *  Implementation  *
     ********************/

    template <typename Iter>
    auto MPool::add_repo_from_packages(
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
    auto MPool::add_repo_from_packages(
        const Range& packages,
        std::string_view name,
        solver::libsolv::PipAsPythonDependency add
    ) -> solver::libsolv::RepoInfo
    {
        return add_repo_from_packages(packages.begin(), packages.end(), name, add);
    }
}

#endif  // MAMBA_POOL_HPP
