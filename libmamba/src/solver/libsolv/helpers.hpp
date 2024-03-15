// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLVER_LIBSOLV_HELPERS
#define MAMBA_SOLVER_LIBSOLV_HELPERS

#include <optional>
#include <string>
#include <string_view>

#include "mamba/core/error_handling.hpp"
#include "mamba/solver/libsolv/parameters.hpp"
#include "mamba/solver/request.hpp"
#include "mamba/solver/solution.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/package_info.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/repo.hpp"
#include "solv-cpp/solvable.hpp"
#include "solv-cpp/transaction.hpp"

#include "solver/libsolv/matcher.hpp"

/**
 * Solver, repo, and solvable helpers dependent on specifi libsolv logic and objects.
 */

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
        bool only_tar_bz2,
        bool verify_artifacts
    ) -> expected_t<solv::ObjRepoView>;

    [[nodiscard]] auto mamba_read_json(
        solv::ObjPool& pool,
        solv::ObjRepoView repo,
        const fs::u8path& filename,
        const std::string& repo_url,
        const std::string& channel_id,
        bool only_tar_bz2,
        bool verify_artifacts
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

    void
    set_solvables_url(solv::ObjRepoView repo, const std::string& repo_url, const std::string& channel_id);

    void add_pip_as_python_dependency(solv::ObjPool& pool, solv::ObjRepoView repo);

    /**
     * Make parameters to use as a namespace dependency.
     *
     * We use these proxy function since we are abusing the two string parameters of namespace
     * callback to pass our own information.
     */
    [[nodiscard]] auto make_abused_namespace_dep_args(
        solv::ObjPool& pool,
        std::string_view dependency,
        const MatchFlags& flags = {}
    ) -> std::pair<solv::StringId, solv::StringId>;

    /**
     * Retrieved parameters used in a namespace callback.
     *
     * We use these proxy function since we are abusing the two string parameters of namespace
     * callback to pass our own information.
     */
    [[nodiscard]] auto get_abused_namespace_callback_args(  //
        solv::ObjPoolView& pool,
        solv::StringId first,
        solv::StringId second
    ) -> std::pair<std::string_view, MatchFlags>;

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

    [[nodiscard]] auto transaction_to_solution_all(  //
        const solv::ObjPool& pool,
        const solv::ObjTransaction& trans
    ) -> Solution;

    [[nodiscard]] auto transaction_to_solution_only_deps(  //
        const solv::ObjPool& pool,
        const solv::ObjTransaction& trans,
        const Request& request
    ) -> Solution;

    [[nodiscard]] auto transaction_to_solution_no_deps(  //
        const solv::ObjPool& pool,
        const solv::ObjTransaction& trans,
        const Request& request
    ) -> Solution;

    [[nodiscard]] auto transaction_to_solution(  //
        const solv::ObjPool& pool,
        const solv::ObjTransaction& trans,
        const Request& request,
        const Request::Flags& flags
    ) -> Solution;

    [[nodiscard]] auto installed_python(const solv::ObjPool& pool)
        -> std::optional<solv::ObjSolvableViewConst>;

    [[nodiscard]] auto solution_needs_python_relink(  //
        const solv::ObjPool& pool,
        const Solution& solution
    ) -> bool;

    [[nodiscard]] auto add_noarch_relink_to_solution(  //
        Solution solution,
        const solv::ObjPool& pool,
        std::string_view noarch_type
    ) -> Solution;

    [[nodiscard]] auto request_to_decision_queue(
        const Request& request,
        solv::ObjPool& pool,
        const specs::ChannelResolveParams& chan_params,
        bool force_reinstall
    ) -> expected_t<solv::ObjQueue>;
}
#endif
