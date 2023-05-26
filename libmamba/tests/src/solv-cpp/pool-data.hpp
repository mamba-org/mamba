// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef TEST_MAMBA_SOLV_POOL_DATA_HPP
#define TEST_MAMBA_SOLV_POOL_DATA_HPP

#include <string>
#include <vector>

#include "solv-cpp/pool.hpp"
#include "solv-cpp/repo.hpp"

namespace mamba::test
{
    struct SimplePkg
    {
        std::string name;
        std::string version;
        std::vector<std::string> dependencies = {};
    };

    inline auto make_simple_packages()
    {
        return std::array{
            SimplePkg{ "menu", "1.5.0", { "dropdown=2.*" } },
            SimplePkg{ "menu", "1.4.0", { "dropdown=2.*" } },
            SimplePkg{ "menu", "1.3.0", { "dropdown=2.*" } },
            SimplePkg{ "menu", "1.2.0", { "dropdown=2.*" } },
            SimplePkg{ "menu", "1.1.0", { "dropdown=2.*" } },
            SimplePkg{ "menu", "1.0.0", { "dropdown=1.*" } },
            SimplePkg{ "dropdown", "2.3.0", { "icons=2.*" } },
            SimplePkg{ "dropdown", "2.2.0", { "icons=2.*" } },
            SimplePkg{ "dropdown", "2.1.0", { "icons=2.*" } },
            SimplePkg{ "dropdown", "2.0.0", { "icons=2.*" } },
            SimplePkg{ "dropdown", "1.8.0", { "icons=1.*", "intl=3.*" } },
            SimplePkg{ "icons", "2.0.0" },
            SimplePkg{ "icons", "1.0.0" },
            SimplePkg{ "intl", "5.0.0" },
            SimplePkg{ "intl", "4.0.0" },
            SimplePkg{ "intl", "3.0.0" },
        };
    }

    inline void add_simple_packages(solv::ObjPool& pool, solv::ObjRepoView& repo)
    {
        for (const auto& pkg : make_simple_packages())
        {
            auto [solv_id, solv] = repo.add_solvable();
            solv.set_name(pkg.name);
            solv.set_version(pkg.version);
            for (const auto& dep : pkg.dependencies)
            {
                solv.add_dependency(pool.add_conda_dependency(dep));
            }
            solv.add_self_provide();
        }
    }
}
#endif
