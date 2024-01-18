// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLVER_LIBSOLV_HERLPERS
#define MAMBA_SOLVER_LIBSOLV_HERLPERS

#include "mamba/core/error_handling.hpp"
#include "mamba/core/solution.hpp"
#include "mamba/solver/libsolv/parameters.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/package_info.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/repo.hpp"
#include "solv-cpp/solvable.hpp"
#include "solv-cpp/transaction.hpp"

namespace mamba::fs
{
    class u8path;
}

namespace mamba::solver::libsolv
{
    void set_solvable(solv::ObjPool& pool, solv::ObjSolvableView solv, const specs::PackageInfo& pkg);

    auto make_package_info(const solv::ObjPool& pool, solv::ObjSolvableViewConst s)
        -> specs::PackageInfo;

    [[nodiscard]] auto libsolv_read_json(  //
        solv::ObjRepoView repo,
        const fs::u8path& filename,
        bool only_tar_bz2
    ) -> expected_t<solv::ObjRepoView>;

    [[nodiscard]] auto mamba_read_json(
        solv::ObjPool& pool,
        solv::ObjRepoView repo,
        const fs::u8path& filename,
        const std::string& repo_url,
        bool only_tar_bz2
    ) -> expected_t<solv::ObjRepoView>;

    [[nodiscard]] auto read_solv(
        solv::ObjPool& pool,
        solv::ObjRepoView repo,
        const fs::u8path& filename,
        const RepodataOrigin& expected,
        bool expected_pip_added
    ) -> expected_t<solv::ObjRepoView>;

    [[nodiscard]] auto write_solv(  //
        solv::ObjRepoView repo,
        fs::u8path filename,
        const RepodataOrigin& metadata
    ) -> expected_t<solv::ObjRepoView>;

    void set_solvables_url(solv::ObjRepoView repo, const std::string& repo_url);

    void add_pip_as_python_dependency(solv::ObjPool& pool, solv::ObjRepoView repo);

    [[nodiscard]] auto pool_add_matchspec(  //
        solv::ObjPool& pool,
        const specs::MatchSpec& ms,
        const specs::ChannelResolveParams& params
    ) -> expected_t<solv::DependencyId>;

    [[nodiscard]] auto pool_add_pin(  //
        solv::ObjPool& pool,
        const specs::MatchSpec& pin_ms,
        const specs::ChannelResolveParams& params
    ) -> expected_t<solv::ObjSolvableView>;

    [[nodiscard]] auto transaction_to_solution(
        const solv::ObjPool& pool,
        const solv::ObjTransaction& trans,
        const util::flat_set<std::string>& specs = {},
        /** true to filter out specs, false to filter in specs */
        bool keep_only = true
    ) -> Solution;
}
#endif
