// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_DEPENDENCY_HPP
#define MAMBA_SOLV_DEPENDENCY_HPP

#include <solv/poolid.h>

#include "solv-cpp/ids.hpp"

namespace solv
{
    class ObjDependencyViewConst
    {
    public:

        explicit ObjDependencyViewConst(const ::Reldep& reldep) noexcept;
        ~ObjDependencyViewConst() noexcept;

        [[nodiscard]] auto raw() const -> const ::Reldep*;

        /**
         * The name field of the dependency.
         *
         * Can be a string id for simple dependencies, or another dependency id for
         * complex depndencies with boolean expressions.
         */
        [[nodiscard]] auto name() const -> StringId /* OR DependencyId */;

        /**
         * The version range field of the dependency.
         *
         * Can be a string id for simple dependencies, or another dependency id for
         * complex depndencies with boolean expressions.
         */
        [[nodiscard]] auto version_range() const -> StringId /* OR DependencyId */;

        /** The flags of the dependency, such as types. */
        [[nodiscard]] auto flags() const -> RelationFlag;

    private:

        const ::Reldep* m_reldep = nullptr;
    };
}
#endif
