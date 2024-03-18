// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_POOL_HPP
#define MAMBA_SOLV_POOL_HPP

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <solv/pool.h>

#include "solv-cpp/ids.hpp"
#include "solv-cpp/queue.hpp"
#include "solv-cpp/repo.hpp"
#include "solv-cpp/solvable.hpp"

namespace solv
{
    class ObjPool;

    /**
     * Pool of solvable involved in resolving en environment.
     *
     * The pool contains the solvable (packages) information required from the ``::Solver``.
     * The pool can be reused by multiple solvers to solve differents requirements with the same
     * ecosystem.
     */
    class ObjPoolView
    {
    public:

        using raw_str_view = const char*;
        using raw_ptr = ::Pool*;
        using const_raw_ptr = const ::Pool*;

        auto raw() -> raw_ptr;
        auto raw() const -> const_raw_ptr;

        auto current_error() const -> std::string_view;

        void set_current_error(raw_str_view msg);
        void set_current_error(const std::string& msg);

        /**
         * Get the current distribution type of the pool.
         *
         * @see ObjPoolView::set_disttype
         */
        auto disttype() const -> DistType;

        /**
         * Set the distribution type of the pool.
         *
         * The distribution type has subtle implications.
         * For instance the distribution must be of type conda for ``track_feature``,
         * ``constrains`` and ``build_number`` to be taken into account.
         */
        void set_disttype(DistType dt);

        /**
         * Find a string id in the pool if it exists.
         *
         * @see ObjPoolView::add_string
         */
        auto find_string(std::string_view str) const -> std::optional<StringId>;

        /**
         * Add a string to the pool.
         *
         * The pool hold a set of strings, indexed with an id, to avoid duplicating entries.
         * It is safe to call this function regardless of whether the string was already added.
         */
        auto add_string(std::string_view str) -> StringId;

        /**
         * Get the string associated with an id.
         *
         * @see ObjPoolView::add_string
         */
        auto get_string(StringId id) const -> std::string_view;

        /**
         * Find a dependency in the pool, if it exists.
         *
         * @see ObjPoolView::add_dependency
         */
        auto find_dependency(StringId name_id, RelationFlag flag, StringId version_id) const
            -> std::optional<DependencyId>;

        /**
         * Add a dependency in the pool.
         *
         * A dependency represents a set of packages.
         * The flag can be used to create complex dependencies. In that case, for instance with
         * the "or" operator, the name and version ids are (ab)used with other dependency ids.
         * Handling of complex dependencies in libsolv is quite complex and not used in mamba.
         */
        auto add_dependency(StringId name_id, RelationFlag flag, StringId version_id) -> DependencyId;

        /**
         * Parse a dependency from string and add it to the pool.
         */
        auto add_conda_dependency(raw_str_view dep) -> DependencyId;
        auto add_conda_dependency(const std::string& dep) -> DependencyId;

        /** Get the registered name of a dependency. */
        auto get_dependency_name(DependencyId id) const -> std::string_view;

        /** Get the registered version of a dependency. */
        auto get_dependency_version(DependencyId id) const -> std::string_view;

        /** Get the registered realtion between a dependency name and version. */
        auto get_dependency_relation(DependencyId id) const -> std::string_view;

        /** Compute the string representation of a dependency. */
        auto dependency_to_string(DependencyId id) const -> std::string;

        /**
         * Create an indexed lookup of dependencies.
         *
         * Create an index to retrieve the list of solvables satisfying a given dependency.
         * This is an expensive operation.
         * The index is also computed over regular ``StringId``, in which case they reprensent
         * all packages that provide that name (without restriction on version).
         */
        void create_whatprovides();

        /**
         * Call @ref create_whatprovides if it does not exists.
         *
         * This does not update the whatprovides index if it was outdated.
         */
        void ensure_whatprovides();

        /**
         * Add an entry on the ``whatprovides_data``.
         *
         * This works in as an input to @ref add_to_whatprovides or @ref set_namespace_callback.
         *
         * @see add_to_whatprovides
         * @see add_to_whatprovides
         */
        auto add_to_whatprovides_data(const ObjQueue& solvables) -> OffsetId;

        auto add_to_whatprovdies_data(const SolvableId* ptr, std::size_t count) -> OffsetId;

        /**
         * Add an entry to ``whatprovides``.
         *
         * This is the table that is looked up to know which solvables satistfy a given dependency.
         * Entries set with this function get overriden by @ref create_whatprovides.
         */
        void add_to_whatprovides(DependencyId dep, OffsetId solvables);

        /**
         * Execute function for each solvable id that provides the given dependency.
         *
         * @pre ObjPoolView::create_whatprovides must have been called before.
         */
        template <typename UnaryFunc>
        void for_each_whatprovides_id(DependencyId dep, UnaryFunc&& func) const;

        /**
         * Execute function for each solvable that provides the given dependency.
         *
         * @pre ObjPoolView::create_whatprovides must have been called before.
         */
        template <typename UnaryFunc>
        void for_each_whatprovides(DependencyId dep, UnaryFunc&& func) const;
        template <typename UnaryFunc>
        void for_each_whatprovides(DependencyId dep, UnaryFunc&& func);

        /**
         * General purpose query of solvables with given attributes.
         *
         * @return A Queue of SolvableId
         */
        auto select_solvables(const ObjQueue& job) const -> ObjQueue;

        /**
         * Find solvables whose dependency in keyname match the dependency.
         *
         * For instance, with ``key=SOLVABLE_REQUIRES``, find solvables for which @p dep satisfies
         * a dependency.
         */
        auto what_matches_dep(KeyNameId key, DependencyId dep, DependencyMarker marker = -1) const
            -> ObjQueue;

        /**
         * Add a repository with a given name.
         *
         * Solvable belong to a repository, although they are stored in the pool.
         */
        auto add_repo(std::string_view name) -> std::pair<RepoId, ObjRepoView>;

        /** Check if a given repoitory id exists. */
        auto has_repo(RepoId id) const -> bool;

        /** Get the repository associated with the given id, if it exists. */
        auto get_repo(RepoId id) -> std::optional<ObjRepoView>;

        /** Get the repository associated with the given id, if it exists. */
        auto get_repo(RepoId id) const -> std::optional<ObjRepoViewConst>;

        /** Return the number of repository in the pool. */
        auto repo_count() const -> std::size_t;

        /**
         * Remove a repository.
         *
         * Repo ids are not invalidated.
         * If @p reuse_ids is true, the solvable ids used in the pool can be reused for future
         * solvables.
         */
        auto remove_repo(RepoId id, bool reuse_ids) -> bool;

        /** Execute function for each repository id in the pool. */
        template <typename UnaryFunc>
        void for_each_repo_id(UnaryFunc&& func) const;

        /** Execute function for each repository in the pool. */
        template <typename UnaryFunc>
        void for_each_repo(UnaryFunc&& func);

        /** Execute function for each repository in the pool. */
        template <typename UnaryFunc>
        void for_each_repo(UnaryFunc&& func) const;

        /**
         * Get the repository of installed packages, if it exists.
         *
         * @see ObjPoolView::set_installed_repository
         */
        auto installed_repo() const -> std::optional<ObjRepoViewConst>;

        /**
         * Get the repository of installed packages, if it exists.
         *
         * @see ObjPoolView::set_installed_repository
         */
        auto installed_repo() -> std::optional<ObjRepoView>;

        /**
         * Set the installed repository.
         *
         * The installed repository represents package already installed.
         * For instance, it is used to filter out the solvable that are alrady available after
         * a solve.
         */
        void set_installed_repo(RepoId id);

        /** Get the number of solvable in the pool, all repo combined. */
        auto solvable_count() const -> std::size_t;

        /** Get a solvable from its id, if it exisists and regardless of its repository. */
        auto get_solvable(SolvableId id) const -> std::optional<ObjSolvableViewConst>;

        /** Get a solvable from its id, if it exisists and regardless of its repository. */
        auto get_solvable(SolvableId id) -> std::optional<ObjSolvableView>;

        /** Execute function for each solvable id in the pool (in all repositories). */
        template <typename UnaryFunc>
        void for_each_solvable_id(UnaryFunc&& func) const;

        /** Execute function for each solvable in the pool (in all repositories). */
        template <typename UnaryFunc>
        void for_each_solvable(UnaryFunc&& func) const;

        /** Execute function for each solvable in the pool (in all repositories). */
        template <typename UnaryFunc>
        void for_each_solvable(UnaryFunc&& func);

        /** Execute function for each solvable id in the installed repository (if it exists). */
        template <typename UnaryFunc>
        void for_each_installed_solvable_id(UnaryFunc&& func) const;

        /** Execute function for each solvable id in the installed repository (if it exists). */
        template <typename UnaryFunc>
        void for_each_installed_solvable(UnaryFunc&& func) const;

        /** Execute function for each solvable id in the installed repository (if it exists). */
        template <typename UnaryFunc>
        void for_each_installed_solvable(UnaryFunc&& func);

        /** Rethrow exception thrown in callback. */
        void rethrow_potential_callback_exception() const;

    protected:

        using UserCallback = std::function<OffsetId(ObjPoolView, StringId, StringId)>;

        /**
         * A wrapper around user callback to handle exceptions.
         *
         * This cannot be set in this class since this is only a view type but can be checked
         * for errors.
         */
        struct NamespaceCallbackWrapper;


    private:

        raw_ptr m_pool;

        ObjPoolView(raw_ptr ptr);

        friend class ObjPool;
    };

    class ObjPool : private ObjPoolView
    {
    public:

        ObjPool();
        ~ObjPool();

        using ObjPoolView::raw;
        using ObjPoolView::current_error;
        using ObjPoolView::set_current_error;
        using ObjPoolView::disttype;
        using ObjPoolView::set_disttype;
        using ObjPoolView::find_string;
        using ObjPoolView::add_string;
        using ObjPoolView::get_string;
        using ObjPoolView::find_dependency;
        using ObjPoolView::add_dependency;
        using ObjPoolView::add_conda_dependency;
        using ObjPoolView::get_dependency_name;
        using ObjPoolView::get_dependency_version;
        using ObjPoolView::get_dependency_relation;
        using ObjPoolView::dependency_to_string;
        using ObjPoolView::create_whatprovides;
        using ObjPoolView::ensure_whatprovides;
        using ObjPoolView::add_to_whatprovides_data;
        using ObjPoolView::add_to_whatprovides;
        using ObjPoolView::for_each_whatprovides_id;
        using ObjPoolView::for_each_whatprovides;
        using ObjPoolView::select_solvables;
        using ObjPoolView::what_matches_dep;
        using ObjPoolView::add_repo;
        using ObjPoolView::has_repo;
        using ObjPoolView::get_repo;
        using ObjPoolView::repo_count;
        using ObjPoolView::remove_repo;
        using ObjPoolView::for_each_repo_id;
        using ObjPoolView::for_each_repo;
        using ObjPoolView::installed_repo;
        using ObjPoolView::set_installed_repo;
        using ObjPoolView::solvable_count;
        using ObjPoolView::get_solvable;
        using ObjPoolView::for_each_solvable_id;
        using ObjPoolView::for_each_solvable;
        using ObjPoolView::for_each_installed_solvable_id;
        using ObjPoolView::for_each_installed_solvable;
        using ObjPoolView::rethrow_potential_callback_exception;

        /** Set the callback to handle libsolv messages.
         *
         * The callback takes a ``Pool*``, the type of message as ``int``, and the message
         * as a ``std::string_view``.
         * The callback must be marked ``noexcept``.
         */
        template <typename Func>
        void set_debug_callback(Func&& callback);

        using UserCallback = ObjPoolView::UserCallback;

        void set_namespace_callback(UserCallback&& callback);

    private:

        struct PoolDeleter
        {
            void operator()(::Pool* ptr);
        };

        std::unique_ptr<void, void (*)(void*)> m_user_debug_callback;
        std::unique_ptr<NamespaceCallbackWrapper> m_user_namespace_callback;
        // Must be deleted before the debug callback
        std::unique_ptr<::Pool, ObjPool::PoolDeleter> m_pool = nullptr;
    };

    /*******************************
     *  Implementation of ObjPoolView  *
     *******************************/

    template <typename UnaryFunc>
    void ObjPoolView::for_each_repo_id(UnaryFunc&& func) const
    {
        const ::Pool* const pool = raw();
        const ::Repo* repo = nullptr;
        RepoId repo_id = 0;
        FOR_REPOS(repo_id, repo)
        {
            if constexpr (std::is_same_v<decltype(func(repo_id)), LoopControl>)
            {
                if (func(repo_id) == LoopControl::Break)
                {
                    break;
                }
            }
            else
            {
                func(repo_id);
            }
        }
    }

    template <typename UnaryFunc>
    void ObjPoolView::for_each_repo(UnaryFunc&& func) const
    {
        // Safe optional unchecked because we iterate over available values
        return for_each_repo_id([this, func](RepoId id) { func(get_repo(id).value()); });
    }

    template <typename UnaryFunc>
    void ObjPoolView::for_each_repo(UnaryFunc&& func)
    {
        // Safe optional unchecked because we iterate over available values
        return for_each_repo_id([this, func](RepoId id) { func(get_repo(id).value()); });
    }

    template <typename UnaryFunc>
    void ObjPoolView::for_each_whatprovides_id(DependencyId dep, UnaryFunc&& func) const
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
            if constexpr (std::is_same_v<decltype(func(id)), LoopControl>)
            {
                if (func(id) == LoopControl::Break)
                {
                    break;
                }
            }
            else
            {
                func(id);
            }
        }
    }

    template <typename UnaryFunc>
    void ObjPoolView::for_each_whatprovides(DependencyId dep, UnaryFunc&& func) const
    {
        // Safe optional unchecked because we iterate over available values
        return for_each_whatprovides_id(
            dep,
            [this, func](SolvableId id) { func(get_solvable(id)).value(); }
        );
    }

    template <typename UnaryFunc>
    void ObjPoolView::for_each_whatprovides(DependencyId dep, UnaryFunc&& func)
    {
        // Safe optional unchecked because we iterate over available values
        return for_each_whatprovides_id(
            dep,
            [this, func](SolvableId id) { func(get_solvable(id).value()); }
        );
    }

    template <typename UnaryFunc>
    void ObjPoolView::for_each_solvable_id(UnaryFunc&& func) const
    {
        const ::Pool* const pool = raw();
        SolvableId id = 0;
        FOR_POOL_SOLVABLES(id)
        {
            if constexpr (std::is_same_v<decltype(func(id)), LoopControl>)
            {
                if (func(id) == LoopControl::Break)
                {
                    break;
                }
            }
            else
            {
                func(id);
            }
        }
    }

    template <typename UnaryFunc>
    void ObjPoolView::for_each_solvable(UnaryFunc&& func) const
    {
        // Safe optional unchecked because we iterate over available values
        return for_each_solvable_id([this, func](SolvableId id) { func(get_solvable(id).value()); });
    }

    template <typename UnaryFunc>
    void ObjPoolView::for_each_solvable(UnaryFunc&& func)
    {
        // Safe optional unchecked because we iterate over available values
        return for_each_solvable_id([this, func](SolvableId id) { func(get_solvable(id).value()); });
    }

    template <typename UnaryFunc>
    void ObjPoolView::for_each_installed_solvable_id(UnaryFunc&& func) const
    {
        if (auto installed = installed_repo(); installed.has_value())
        {
            installed->for_each_solvable_id(std::forward<UnaryFunc>(func));
        }
    }

    template <typename UnaryFunc>
    void ObjPoolView::for_each_installed_solvable(UnaryFunc&& func) const
    {
        if (auto installed = installed_repo(); installed.has_value())
        {
            installed->for_each_solvable(std::forward<UnaryFunc>(func));
        }
    }

    template <typename UnaryFunc>
    void ObjPoolView::for_each_installed_solvable(UnaryFunc&& func)
    {
        if (auto installed = installed_repo(); installed.has_value())
        {
            installed->for_each_solvable(std::forward<UnaryFunc>(func));
        }
    }

    template <typename Func>
    void ObjPool::set_debug_callback(Func&& callback)
    {
        static_assert(
            std::is_nothrow_invocable_v<Func, ObjPoolView, int, std::string_view>,
            "User callback must be marked noexcept."
        );

        m_user_debug_callback.reset(new Func(std::forward<Func>(callback)));
        m_user_debug_callback.get_deleter() = [](void* ptr) { delete reinterpret_cast<Func*>(ptr); };

        // Wrap the user callback in the libsolv function type that must cast the callback ptr
        auto debug_callback = [](Pool* pool, void* user_data, int type, const char* msg) noexcept
        {
            auto* user_debug_callback = reinterpret_cast<Func*>(user_data);
            (*user_debug_callback)(ObjPoolView(pool), type, std::string_view(msg));  // noexcept
        };

        ::pool_setdebugcallback(raw(), debug_callback, m_user_debug_callback.get());
    }
}
#endif
