// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_POOL_HPP
#define MAMBA_SOLV_POOL_HPP

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "solv-cpp/ids.hpp"
#include "solv-cpp/queue.hpp"
#include "solv-cpp/repo.hpp"
#include "solv-cpp/solvable.hpp"

using Pool = struct s_Pool;

namespace mamba::solv
{
    /**
     * Pool of solvable involved in resolving en environment.
     *
     * The pool contains the solvable (packages) information required from the ``::Solver``.
     * The pool can be reused by multiple solvers to solve differents requirements with the same
     * ecosystem.
     */
    class ObjPool
    {
    public:

        ObjPool();
        ~ObjPool();

        auto raw() -> ::Pool*;
        auto raw() const -> const ::Pool*;

        auto disttype() const -> DistType;
        void set_disttype(DistType dt);

        auto find_string(std::string_view str) const -> std::optional<StringId>;
        auto add_string(std::string_view str) -> StringId;
        auto get_string(StringId id) const -> std::string_view;

        auto find_dependency(StringId name_id, RelationFlag flag, StringId version_id) const
            -> std::optional<DependencyId>;
        auto add_dependency(StringId name_id, RelationFlag flag, StringId version_id) -> DependencyId;
        auto get_dependency_name(DependencyId id) const -> std::string_view;
        auto get_dependency_version(DependencyId id) const -> std::string_view;
        auto get_dependency_relation(DependencyId id) const -> std::string_view;
        auto dependency_to_string(DependencyId id) const -> std::string;

        void create_whatprovides();
        template <typename UnaryFunc>
        void for_each_whatprovides_id(DependencyId dep, UnaryFunc&& func) const;
        template <typename UnaryFunc>
        void for_each_whatprovides(DependencyId dep, UnaryFunc&& func) const;
        template <typename UnaryFunc>
        void for_each_whatprovides(DependencyId dep, UnaryFunc&& func);

        auto select_solvables(const ObjQueue& job) const -> ObjQueue;

        auto add_repo(std::string_view name) -> std::pair<RepoId, ObjRepoView>;
        auto has_repo(RepoId id) const -> bool;
        auto get_repo(RepoId id) -> std::optional<ObjRepoView>;
        auto get_repo(RepoId id) const -> std::optional<ObjRepoViewConst>;
        auto repo_count() const -> std::size_t;
        auto remove_repo(RepoId id, bool reuse_ids) -> bool;
        template <typename UnaryFunc>
        void for_each_repo_id(UnaryFunc&& func) const;
        template <typename UnaryFunc>
        void for_each_repo(UnaryFunc&& func);
        template <typename UnaryFunc>
        void for_each_repo(UnaryFunc&& func) const;
        auto installed_repo() const -> std::optional<ObjRepoViewConst>;
        auto installed_repo() -> std::optional<ObjRepoView>;
        void set_installed_repo(RepoId id);

        auto get_solvable(SolvableId id) const -> std::optional<ObjSolvableViewConst>;
        auto get_solvable(SolvableId id) -> std::optional<ObjSolvableView>;
        template <typename UnaryFunc>
        void for_each_solvable_id(UnaryFunc&& func) const;
        template <typename UnaryFunc>
        void for_each_solvable(UnaryFunc&& func) const;
        template <typename UnaryFunc>
        void for_each_solvable(UnaryFunc&& func);

        template <typename Func>
        void set_debug_callback(Func&& callback);

    private:

        struct PoolDeleter
        {
            void operator()(::Pool* ptr);
        };

        std::unique_ptr<void, void (*)(void*)> m_user_debug_callback;
        // Must be deleted before the debug callback
        std::unique_ptr<::Pool, ObjPool::PoolDeleter> m_pool = nullptr;
    };
}

/*******************************
 *  Implementation of ObjPool  *
 *******************************/

#include <stdexcept>

#include <solv/pool.h>

namespace mamba::solv
{

    template <typename UnaryFunc>
    void ObjPool::for_each_repo_id(UnaryFunc&& func) const
    {
        const ::Pool* const pool = raw();
        const ::Repo* repo = nullptr;
        RepoId repo_id = 0;
        FOR_REPOS(repo_id, repo)
        {
            func(repo_id);
        }
    }

    template <typename UnaryFunc>
    void ObjPool::for_each_repo(UnaryFunc&& func) const
    {
        // Safe optional unchecked because we iterate over available values
        return for_each_repo_id([this, func](RepoId id) { func(get_repo(id).value()); });
    }

    template <typename UnaryFunc>
    void ObjPool::for_each_repo(UnaryFunc&& func)
    {
        // Safe optional unchecked because we iterate over available values
        return for_each_repo_id([this, func](RepoId id) { func(get_repo(id).value()); });
    }

    template <typename UnaryFunc>
    void ObjPool::for_each_whatprovides_id(DependencyId dep, UnaryFunc&& func) const
    {
        if (raw()->whatprovides == nullptr)
        {
            throw std::runtime_error("Whatprovides index is not created");
        }
        auto* const pool = const_cast<::Pool*>(raw());
        SolvableId id = 0;
        ::Id offset = 0;  // Not really an Id
        FOR_PROVIDES(id, offset, dep)
        {
            func(id);
        }
    }

    template <typename UnaryFunc>
    void ObjPool::for_each_whatprovides(DependencyId dep, UnaryFunc&& func) const
    {
        // Safe optional unchecked because we iterate over available values
        return for_each_whatprovides_id(
            dep,
            [this, func](SolvableId id) { func(get_solvable(id)).value(); }
        );
    }

    template <typename UnaryFunc>
    void ObjPool::for_each_whatprovides(DependencyId dep, UnaryFunc&& func)
    {
        // Safe optional unchecked because we iterate over available values
        return for_each_whatprovides_id(
            dep,
            [this, func](SolvableId id) { func(get_solvable(id).value()); }
        );
    }

    template <typename UnaryFunc>
    void ObjPool::for_each_solvable_id(UnaryFunc&& func) const
    {
        const ::Pool* const pool = raw();
        SolvableId id = 0;
        FOR_POOL_SOLVABLES(id)
        {
            func(id);
        }
    }

    template <typename UnaryFunc>
    void ObjPool::for_each_solvable(UnaryFunc&& func) const
    {
        // Safe optional unchecked because we iterate over available values
        return for_each_solvable_id([this, func](SolvableId id) { func(get_solvable(id).value()); });
    }

    template <typename UnaryFunc>
    void ObjPool::for_each_solvable(UnaryFunc&& func)
    {
        // Safe optional unchecked because we iterate over available values
        return for_each_solvable_id([this, func](SolvableId id) { func(get_solvable(id).value()); });
    }

    template <typename Func>
    void ObjPool::set_debug_callback(Func&& callback)
    {
        static_assert(
            std::is_nothrow_invocable_v<Func, Pool*, int, std::string_view>,
            "User callback must be marked noexcept."
        );

        m_user_debug_callback.reset(new Func(std::forward<Func>(callback)));
        m_user_debug_callback.get_deleter() = [](void* ptr) { delete reinterpret_cast<Func*>(ptr); };

        // Wrap the user callback in the libsolv function type that must cast the callback ptr
        auto debug_callback = [](Pool* pool, void* user_data, int type, const char* msg) noexcept
        {
            auto* user_debug_callback = reinterpret_cast<Func*>(user_data);
            (*user_debug_callback)(pool, type, std::string_view(msg));  // noexcept
        };

        pool_setdebugcallback(raw(), debug_callback, m_user_debug_callback.get());
    }
}
#endif
