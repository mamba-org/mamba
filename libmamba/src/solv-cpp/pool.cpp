// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <limits>
#include <sstream>
#include <stdexcept>

#include <solv/pool.h>
#include <solv/poolid.h>
#include <solv/pooltypes.h>
#include <solv/repo.h>
#include <solv/selection.h>
extern "C"  // Incomplete header
{
#include <solv/conda.h>
}

#include "solv-cpp/pool.hpp"

namespace mamba::solv
{
    void ObjPool::PoolDeleter::operator()(::Pool* ptr)
    {
        ::pool_free(ptr);
    }

    ObjPool::ObjPool()
        : m_user_debug_callback(nullptr, [](void* /*ptr*/) {})
        , m_pool(::pool_create())
    {
    }

    ObjPool::~ObjPool() = default;

    auto ObjPool::raw() -> ::Pool*
    {
        return m_pool.get();
    }

    auto ObjPool::raw() const -> const ::Pool*
    {
        return m_pool.get();
    }


    auto ObjPool::disttype() const -> DistType
    {
        return raw()->disttype;
    }

    void ObjPool::set_disttype(DistType dt)
    {
        pool_setdisttype(raw(), dt);
    }

    auto ObjPool::find_string(std::string_view str) const -> std::optional<StringId>
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

    auto ObjPool::add_string(std::string_view str) -> StringId
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
        auto is_reldep(::Id id) -> bool
        {
            return ISRELDEP(static_cast<std::make_unsigned_t<::Id>>(id)) != 0;
        }
    }

    auto ObjPool::get_string(StringId id) const -> std::string_view
    {
        assert(!is_reldep(id));
        return ::pool_id2str(raw(), id);
    }

    auto ObjPool::find_dependency(StringId name_id, RelationFlag flag, StringId version_id) const
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

    auto ObjPool::add_dependency(StringId name_id, RelationFlag flag, StringId version_id)
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

    auto ObjPool::add_conda_dependency(raw_str_view dep) -> DependencyId
    {
        if (const auto id = ::pool_conda_matchspec(raw(), dep); id != 0)
        {
            return id;
        }
        auto msg = std::stringstream{};
        msg << R"(Invalid conda dependency: ")" << dep << '"';
        throw std::invalid_argument(msg.str());
    }

    auto ObjPool::add_conda_dependency(const std::string& dep) -> DependencyId
    {
        return add_conda_dependency(dep.c_str());
    }

    auto ObjPool::get_dependency_name(DependencyId id) const -> std::string_view
    {
        return ::pool_id2str(raw(), id);
    }

    auto ObjPool::get_dependency_version(DependencyId id) const -> std::string_view
    {
        return ::pool_id2evr(raw(), id);
    }

    auto ObjPool::get_dependency_relation(DependencyId id) const -> std::string_view
    {
        return ::pool_id2rel(raw(), id);
    }

    auto ObjPool::dependency_to_string(DependencyId id) const -> std::string
    {
        return ::pool_dep2str(
            // Not const in because function may allocate in pool tmp alloc space
            const_cast<::Pool*>(raw()),
            id
        );
    }

    auto ObjPool::select_solvables(const ObjQueue& job) const -> ObjQueue
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

    void ObjPool::create_whatprovides()
    {
        ::pool_createwhatprovides(raw());
    }

    auto ObjPool::add_to_whatprovides_data(const ObjQueue& solvables) -> OffsetId
    {
        return add_to_whatprovdies_data(solvables.data(), solvables.size());
    }

    auto ObjPool::add_to_whatprovdies_data(const SolvableId* ptr, std::size_t count) -> OffsetId
    {
        assert(count <= std::numeric_limits<int>::max());
        if (raw()->whatprovidesdata == nullptr)
        {
            throw std::runtime_error("Whatprovides index is not created");
        }
        return ::pool_ids2whatprovides(raw(), const_cast<::Id*>(ptr), static_cast<int>(count));
    }

    void ObjPool::add_to_whatprovides(DependencyId dep, OffsetId solvables)
    {
        if (raw()->whatprovides == nullptr)
        {
            throw std::runtime_error("Whatprovides index is not created");
        }
        ::pool_set_whatprovides(raw(), dep, solvables);
    }

    auto ObjPool::add_repo(std::string_view name) -> std::pair<RepoId, ObjRepoView>
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

    auto ObjPool::has_repo(RepoId id) const -> bool
    {
        return (id > 0) && (id < raw()->nrepos) && (raw()->repos[id] != nullptr);
    }

    auto ObjPool::get_repo(RepoId id) -> std::optional<ObjRepoView>
    {
        if (!has_repo(id))
        {
            return std::nullopt;
        }
        auto* repo_ptr = ::pool_id2repo(raw(), id);
        assert(repo_ptr != nullptr);
        return { ObjRepoView{ *repo_ptr } };
    }

    auto ObjPool::get_repo(RepoId id) const -> std::optional<ObjRepoViewConst>
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

    auto ObjPool::repo_count() const -> std::size_t
    {
        // Id 0 is special
        assert(raw()->urepos >= 0);
        return static_cast<std::size_t>(raw()->urepos);
    }

    auto ObjPool::remove_repo(RepoId id, bool reuse_ids) -> bool
    {
        if (has_repo(id))
        {
            ::repo_free(get_repo(id).value().raw(), static_cast<int>(reuse_ids));
            return true;
        }
        return false;
    }

    auto ObjPool::installed_repo() const -> std::optional<ObjRepoViewConst>
    {
        if (const auto* const installed_ptr = raw()->installed)
        {
            return ObjRepoViewConst{ *installed_ptr };
        }
        return std::nullopt;
    }

    auto ObjPool::installed_repo() -> std::optional<ObjRepoView>
    {
        if (auto* const installed_ptr = raw()->installed)
        {
            return ObjRepoView{ *installed_ptr };
        }
        return std::nullopt;
    }

    void ObjPool::set_installed_repo(RepoId id)
    {
        const auto must_repo = get_repo(id);
        assert(must_repo.has_value());
        pool_set_installed(raw(), must_repo->raw());
    }

    inline static constexpr int solvable_id_start = 2;

    auto ObjPool::solvable_count() const -> std::size_t
    {
        assert(raw()->nsolvables >= solvable_id_start);
        return static_cast<std::size_t>(raw()->nsolvables - solvable_id_start);
    }

    auto ObjPool::get_solvable(SolvableId id) const -> std::optional<ObjSolvableViewConst>
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

    auto ObjPool::get_solvable(SolvableId id) -> std::optional<ObjSolvableView>
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
}
