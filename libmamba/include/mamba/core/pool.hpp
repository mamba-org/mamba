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
#include "mamba/core/repo.hpp"
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
            MRepo::PipAsPythonDependency add = MRepo::PipAsPythonDependency::No,
            MRepo::RepodataParser parser = MRepo::RepodataParser::Mamba
        ) -> expected_t<MRepo>;

        auto add_repo_from_native_serialization(
            const fs::u8path& path,
            const solver::libsolv::RepodataOrigin& expected,
            MRepo::PipAsPythonDependency add = MRepo::PipAsPythonDependency::No
        ) -> expected_t<MRepo>;

        void native_serialize_repo(
            const MRepo& repo,
            const fs::u8path& path,
            const solver::libsolv::RepodataOrigin& metadata
        );

        void set_installed_repo(const MRepo& repo);

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
    };

    // TODO machinery functions in separate files
    auto load_subdir_in_pool(const Context& ctx, MPool& pool, const MSubdirData& subdir)
        -> expected_t<MRepo>;
}

#endif  // MAMBA_POOL_HPP
