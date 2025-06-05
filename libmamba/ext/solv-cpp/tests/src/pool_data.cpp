// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <tuple>

#include "pool_data.hpp"

namespace solv::test
{
    namespace
    {
        auto attrs(const SimplePkg& p)
        {
            return std::tie(p.name, p.version, p.dependencies);
        }
    }

    auto operator==(const SimplePkg& a, const SimplePkg& b) -> bool
    {
        return attrs(a) == attrs(b);
    }

    auto operator!=(const SimplePkg& a, const SimplePkg& b) -> bool
    {
        return attrs(a) != attrs(b);
    }

    auto operator<(const SimplePkg& a, const SimplePkg& b) -> bool
    {
        return attrs(a) < attrs(b);
    }

    auto operator<=(const SimplePkg& a, const SimplePkg& b) -> bool
    {
        return attrs(a) <= attrs(b);
    }

    auto operator>(const SimplePkg& a, const SimplePkg& b) -> bool
    {
        return attrs(a) > attrs(b);
    }

    auto operator>=(const SimplePkg& a, const SimplePkg& b) -> bool
    {
        return attrs(a) >= attrs(b);
    }

    auto add_simple_package(solv::ObjPool& pool, solv::ObjRepoView& repo, const SimplePkg& pkg)
        -> solv::SolvableId
    {
        auto [solv_id, solv] = repo.add_solvable();
        solv.set_name(pkg.name);
        solv.set_version(pkg.version);
        for (const auto& dep : pkg.dependencies)
        {
            solv.add_dependency(pool.add_legacy_conda_dependency(dep));
        }
        solv.add_self_provide();
        return solv_id;
    }
}
