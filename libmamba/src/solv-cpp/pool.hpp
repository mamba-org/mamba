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

#include "solv-cpp/ids.hpp"
#include "solv-cpp/queue.hpp"

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

        auto select_solvables(const ObjQueue& job) const -> ObjQueue;

    private:

        struct PoolDeleter
        {
            void operator()(::Pool* ptr);
        };

        std::unique_ptr<::Pool, ObjPool::PoolDeleter> m_pool;
    };
}
#endif
