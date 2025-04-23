// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <exception>
#include <limits>
#include <memory>
#include <stdexcept>

#include <solv/conda.h>
#include <solv/pool.h>
#include <solv/poolid.h>
#include <solv/pooltypes.h>
#include <solv/repo.h>
#include <solv/selection.h>

#include "solv-cpp/pool.hpp"

namespace solv
{
    /***********************************
     *  Implementation of ObjPoolView  *
     ***********************************/

    ObjPoolView::ObjPoolView(raw_ptr ptr)
        : m_pool(ptr)
    {
    }

    auto ObjPoolView::raw() -> raw_ptr
    {
        return m_pool;
    }

    auto ObjPoolView::raw() const -> const_raw_ptr
    {
        return m_pool;
    }

    auto ObjPoolView::current_error() const -> std::string_view
    {
        return ::pool_errstr(m_pool);
    }

    void ObjPoolView::set_current_error(raw_str_view msg)
    {
        ::pool_error(m_pool, -1, "%s", msg);
    }

    void ObjPoolView::set_current_error(const std::string& msg)
    {
        return set_current_error(msg.c_str());
    }

    auto ObjPoolView::disttype() const -> DistType
    {
        return raw()->disttype;
    }

    void ObjPoolView::set_disttype(DistType dt)
    {
        pool_setdisttype(raw(), dt);
    }

    auto ObjPoolView::find_string(std::string_view str) const -> std::optional<StringId>
    {
        assert(str.size() <= std::numeric_limits<unsigned int>::max());
        const ::Id id = ::pool_strn2id(
            const_cast<::Pool*>(raw()),  // Safe because we do not create
            str.data(),
            static_cast<unsigned int>(str.size()),
            /* .create= */ 0
        );
        return (id == 0) ? std::nullopt : std::optional(id);
    }

    auto ObjPoolView::add_string(std::string_view str) -> StringId
    {
        assert(str.size() <= std::numeric_limits<unsigned int>::max());
        // Note: libsolv cannot report failure to allocate
        const ::Id id = ::pool_strn2id(
            raw(),
            str.data(),
            static_cast<unsigned int>(str.size()),
            /* .create= */ 1
        );
        assert(id != 0);
        return id;
    }

    namespace
    {
        [[nodiscard]] auto is_reldep(::Id id) -> bool
        {
            return ISRELDEP(static_cast<std::make_unsigned_t<::Id>>(id)) != 0;
        }

        [[nodiscard]] auto get_reldep(const ::Pool* pool, ::Id id) -> const ::Reldep*
        {
            return GETRELDEP(pool, id);
        }
    }

    auto ObjPoolView::get_string(StringId id) const -> std::string_view
    {
        assert(!is_reldep(id));
        return ::pool_id2str(raw(), id);
    }

    auto ObjPoolView::find_dependency(StringId name_id, RelationFlag flag, StringId version_id) const
        -> std::optional<DependencyId>
    {
        const ::Id id = ::pool_rel2id(
            const_cast<::Pool*>(raw()),  // Safe because we do not create
            name_id,
            version_id,
            flag,
            /* .create= */ 0
        );
        return (id == 0) ? std::nullopt : std::optional(DependencyId{ id });
    }

    auto ObjPoolView::add_dependency(StringId name_id, RelationFlag flag, StringId version_id)
        -> DependencyId
    {
        // Note: libsolv cannot report failure to allocate
        const ::Id id = ::pool_rel2id(
            raw(),
            name_id,
            version_id,
            flag,
            /* .create= */ 1
        );
        assert(id != 0);
        assert(is_reldep(id));
        return id;
    }

    auto ObjPoolView::add_conda_dependency(raw_str_view dep) -> DependencyId
    {
        return ::pool_conda_matchspec(raw(), dep);
    }

    auto ObjPoolView::add_conda_dependency(const std::string& dep) -> DependencyId
    {
        return add_conda_dependency(dep.c_str());
    }

    auto ObjPoolView::get_dependency(DependencyId id) const -> std::optional<ObjDependencyViewConst>
    {
        if (!is_reldep(id))
        {
            return {};
        }
        const auto rel = get_reldep(raw(), id);
        assert(rel != nullptr);
        return { ObjDependencyViewConst(*rel) };
    }

    auto ObjPoolView::get_dependency_name(DependencyId id) const -> std::string_view
    {
        return ::pool_id2str(raw(), id);
    }

    auto ObjPoolView::get_dependency_version(DependencyId id) const -> std::string_view
    {
        return ::pool_id2evr(raw(), id);
    }

    auto ObjPoolView::get_dependency_relation(DependencyId id) const -> std::string_view
    {
        return ::pool_id2rel(raw(), id);
    }

    auto ObjPoolView::dependency_to_string(DependencyId id) const -> std::string
    {
        return ::pool_dep2str(
            // Not const in because function may allocate in pool tmp alloc space
            const_cast<::Pool*>(raw()),
            id
        );
    }

    auto ObjPoolView::select_solvables(const ObjQueue& job) const -> ObjQueue
    {
        ObjQueue solvables = {};
        ::selection_solvables(
            // Not const in because function may allocate in pool tmp alloc space
            const_cast<::Pool*>(raw()),
            // Queue selection param is not modified
            const_cast<::Queue*>(job.raw()),
            solvables.raw()
        );
        return solvables;
    }

    auto ObjPoolView::what_matches_dep(KeyNameId key, DependencyId dep, DependencyMarker marker) const
        -> ObjQueue
    {
        ObjQueue solvables = {};
        ::pool_whatmatchesdep(const_cast<::Pool*>(raw()), key, dep, solvables.raw(), marker);
        return solvables;
    }

    void ObjPoolView::create_whatprovides()
    {
        ::pool_createwhatprovides(raw());
    }

    void ObjPoolView::ensure_whatprovides()
    {
        if (!raw()->whatprovides)
        {
            ::pool_createwhatprovides(raw());
        }
    }

    auto ObjPoolView::add_to_whatprovides_data(const ObjQueue& solvables) -> OffsetId
    {
        return add_to_whatprovdies_data(solvables.data(), solvables.size());
    }

    auto ObjPoolView::add_to_whatprovdies_data(const SolvableId* ptr, std::size_t count) -> OffsetId
    {
        assert(count <= std::numeric_limits<int>::max());
        if (raw()->whatprovidesdata == nullptr)
        {
            throw std::runtime_error("Whatprovides index is not created");
        }
        return ::pool_ids2whatprovides(raw(), const_cast<::Id*>(ptr), static_cast<int>(count));
    }

    void ObjPoolView::add_to_whatprovides(DependencyId dep, OffsetId solvables)
    {
        if (raw()->whatprovides == nullptr)
        {
            throw std::runtime_error("Whatprovides index is not created");
        }
        ::pool_set_whatprovides(raw(), dep, solvables);
    }

    auto ObjPoolView::add_repo(std::string_view name) -> std::pair<RepoId, ObjRepoView>
    {
        auto* repo_ptr = ::repo_create(
            raw(),
            ::pool_id2str(raw(), add_string(name))  // Shenanigan to make it string_view compatible
        );
        // No function exists to create a repo id
        assert(raw()->repos[raw()->nrepos - 1] == repo_ptr);
        assert(repo_ptr != nullptr);
        return { raw()->nrepos - 1, ObjRepoView{ *repo_ptr } };
    }

    auto ObjPoolView::has_repo(RepoId id) const -> bool
    {
        return (id > 0) && (id < raw()->nrepos) && (raw()->repos[id] != nullptr);
    }

    auto ObjPoolView::get_repo(RepoId id) -> std::optional<ObjRepoView>
    {
        if (!has_repo(id))
        {
            return std::nullopt;
        }
        auto* repo_ptr = ::pool_id2repo(raw(), id);
        assert(repo_ptr != nullptr);
        return { ObjRepoView{ *repo_ptr } };
    }

    auto ObjPoolView::get_repo(RepoId id) const -> std::optional<ObjRepoViewConst>
    {
        if (!has_repo(id))
        {
            return std::nullopt;
        }
        // Safe const_cast because we make the Repo deep const
        const auto* repo_ptr = ::pool_id2repo(const_cast<::Pool*>(raw()), id);
        assert(repo_ptr != nullptr);
        return { ObjRepoViewConst{ *repo_ptr } };
    }

    auto ObjPoolView::repo_count() const -> std::size_t
    {
        // Id 0 is special
        assert(raw()->urepos >= 0);
        return static_cast<std::size_t>(raw()->urepos);
    }

    auto ObjPoolView::remove_repo(RepoId id, bool reuse_ids) -> bool
    {
        if (has_repo(id))
        {
            ::repo_free(get_repo(id).value().raw(), static_cast<int>(reuse_ids));
            return true;
        }
        return false;
    }

    auto ObjPoolView::installed_repo() const -> std::optional<ObjRepoViewConst>
    {
        if (const auto* const installed_ptr = raw()->installed)
        {
            return ObjRepoViewConst{ *installed_ptr };
        }
        return std::nullopt;
    }

    auto ObjPoolView::installed_repo() -> std::optional<ObjRepoView>
    {
        if (auto* const installed_ptr = raw()->installed)
        {
            return ObjRepoView{ *installed_ptr };
        }
        return std::nullopt;
    }

    void ObjPoolView::set_installed_repo(RepoId id)
    {
        const auto must_repo = get_repo(id);
        assert(must_repo.has_value());
        pool_set_installed(raw(), must_repo->raw());
    }

    inline static constexpr int solvable_id_start = 2;

    auto ObjPoolView::solvable_count() const -> std::size_t
    {
        assert(raw()->nsolvables >= solvable_id_start);
        return static_cast<std::size_t>(raw()->nsolvables - solvable_id_start);
    }

    auto ObjPoolView::get_solvable(SolvableId id) const -> std::optional<ObjSolvableViewConst>
    {
        if ((solvable_id_start <= id) && (id < raw()->nsolvables))
        {
            if (const ::Solvable* s = ::pool_id2solvable(raw(), id))
            {
                return ObjSolvableViewConst{ *s };
            }
        }
        return std::nullopt;
    }

    auto ObjPoolView::get_solvable(SolvableId id) -> std::optional<ObjSolvableView>
    {
        if ((solvable_id_start <= id) && (id < raw()->nsolvables))
        {
            if (::Solvable* s = ::pool_id2solvable(raw(), id))
            {
                return ObjSolvableView{ *s };
            }
        }
        return std::nullopt;
    }

    struct ObjPoolView::NamespaceCallbackWrapper
    {
        UserCallback callback;
        std::exception_ptr error = nullptr;
    };

    void ObjPoolView::rethrow_potential_callback_exception() const
    {
        if (auto callback = reinterpret_cast<NamespaceCallbackWrapper*>(raw()->nscallbackdata))
        {
            if (auto error = callback->error)
            {
                callback->error = nullptr;
                std::rethrow_exception(error);
            }
        }
    }

    /*******************************
     *  Implementation of ObjPool  *
     *******************************/

    void ObjPool::PoolDeleter::operator()(::Pool* ptr)
    {
        ::pool_free(ptr);
    }

    ObjPool::ObjPool()
        : ObjPoolView(nullptr)
        , m_user_debug_callback(nullptr, [](void* /*ptr*/) {})
        , m_user_namespace_callback(nullptr)
        , m_pool(::pool_create())
    {
        ObjPoolView::m_pool = m_pool.get();
    }

    ObjPool::~ObjPool() = default;

    auto ObjPool::view() const -> ObjPoolView
    {
        return *static_cast<const ObjPoolView*>(this);
    }

    void ObjPool::set_namespace_callback(UserCallback&& callback)
    {
        m_user_namespace_callback = std::make_unique<NamespaceCallbackWrapper>();

        // Set the callback
        m_user_namespace_callback->callback = [wrapper = m_user_namespace_callback.get(),
                                               callback = std::move(callback
                                               )](ObjPoolView pool, StringId name, StringId ver
                                              ) mutable noexcept -> OffsetId
        {
            auto error = std::exception_ptr(nullptr);
            try
            {
                std::swap(error, wrapper->error);
                return callback(pool, name, ver);
            }
            catch (...)
            {
                wrapper->error = std::current_exception();
                return 0;
            }
        };

        // Wrap the user callback in the libsolv function type that must cast the callback ptr
        auto libsolv_callback = +[](::Pool* pool, void* user_data, StringId name, StringId ver
                                 ) noexcept -> OffsetId
        {
            auto* user_namespace_callback = reinterpret_cast<NamespaceCallbackWrapper*>(user_data);
            return user_namespace_callback->callback(ObjPoolView(pool), name, ver);  // noexcept
        };

        ::pool_setnamespacecallback(raw(), libsolv_callback, m_user_namespace_callback.get());
    }
}
