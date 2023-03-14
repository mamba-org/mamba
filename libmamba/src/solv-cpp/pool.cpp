// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "solv-cpp/pool.hpp"

#include <cassert>
#include <limits>

#include <solv/pool.h>
#include <solv/poolid.h>
#include <solv/pooltypes.h>
#include <solv/selection.h>

namespace mamba::solv
{
    void ObjPool::PoolDeleter::operator()(::Pool* ptr)
    {
        ::pool_free(ptr);
    }

    ObjPool::ObjPool()
        : m_pool(::pool_create())
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

    auto ObjPool::find_string(std::string_view str) const -> std::optional<StringId>
    {
        assert(str.size() <= std::numeric_limits<unsigned int>::max());
        ::Id const id = pool_strn2id(
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
        ::Id const id = pool_strn2id(
            raw(),
            str.data(),
            static_cast<unsigned int>(str.size()),
            /* .create= */ 1
        );
        assert(id != 0);
        return id;
    }

    auto ObjPool::get_string(StringId id) const -> std::string_view
    {
        assert(!ISRELDEP(id));
        return pool_id2str(raw(), id);
    }

    auto ObjPool::find_dependency(StringId name_id, RelationFlag flag, StringId version_id) const
        -> std::optional<DependencyId>
    {
        ::Id const id = pool_rel2id(
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
        ::Id const id = pool_rel2id(
            raw(),
            name_id,
            version_id,
            flag,
            /* .create= */ 1
        );
        assert(id != 0);
        assert(ISRELDEP(id));
        return id;
    }

    auto ObjPool::get_dependency_name(DependencyId id) const -> std::string_view
    {
        assert(ISRELDEP(id));
        return pool_id2str(raw(), id);
    }

    auto ObjPool::get_dependency_version(DependencyId id) const -> std::string_view
    {
        assert(ISRELDEP(id));
        return pool_id2evr(raw(), id);
    }

    auto ObjPool::get_dependency_relation(DependencyId id) const -> std::string_view
    {
        assert(ISRELDEP(id));
        return pool_id2rel(raw(), id);
    }

    auto ObjPool::dependency_to_string(DependencyId id) const -> std::string
    {
        assert(ISRELDEP(id));
        return pool_dep2str(
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
        pool_createwhatprovides(raw());
    }
}
