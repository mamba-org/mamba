// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTILS_HPP
#define MAMBA_UTILS_HPP

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "mamba/solver/libsolv/solver.hpp"
#include "mamba/specs/version.hpp"

#include "tl/expected.hpp"

namespace mamba
{
    class ChannelContext;
    class Configuration;
    class Context;
    class MTransaction;
    class PrefixData;
    class MultiPackageCache;
    struct Palette;

    namespace solver
    {
        struct Request;

        namespace libsolv
        {
            class Database;
        }
    }

    namespace detail
    {
        struct other_pkg_mgr_spec;
    }

    namespace pip
    {
        enum class Update : bool
        {
            No = false,
            Yes = true,
        };
    }

    using command_args = std::vector<std::string>;
    inline constexpr std::string_view fallback_python_minor = "3.14";

    /**
     * Build the command-line invocation for a secondary package manager (e.g. pip/uv).
     */
    tl::expected<command_args, std::runtime_error> install_for_other_pkgmgr(
        const Context& ctx,
        const detail::other_pkg_mgr_spec& other_spec,
        pip::Update update
    );

    /**
     * Populate context channels from user-provided specs.
     */
    void
    populate_context_channels_from_specs(const std::vector<std::string>& raw_matchspecs, Context& context);

    /**
     * Extract package names from matchspec strings.
     * Only extracts exact name matches (no version constraints).
     */
    std::vector<std::string> extract_package_names_from_specs(const std::vector<std::string>& specs);

    /**
     * Ensure that ``"pip"`` is present in ``root_packages`` when ``"python"`` is requested.
     *
     * This is used by both install and update flows to automatically add ``pip`` when
     * ``python`` is part of the requested specs, unless ``pip`` is already present.
     */
    void add_pip_if_python(std::vector<std::string>& root_packages);

    /**
     * Build root packages for sharded repodata loading.
     *
     * Starts from requested specs and adds ``pip`` when ``python`` is present.
     */
    std::vector<std::string> build_sharded_root_packages(const std::vector<std::string>& raw_specs);

    /**

     * Print environment activation guidance for the current target prefix.
     */
    void print_activation_message(const Context& ctx);

    /**
     * Solve a request and render the solver status through the current console.
     */
    solver::libsolv::Solver::Outcome solve_request_with_status(
        bool experimental_matchspec_parsing,
        solver::libsolv::Database& db,
        const solver::Request& request
    );

    /**
     * Create a libsolv database configured for the current matching behavior.
     */
    solver::libsolv::Database
    make_solver_database(bool experimental_matchspec_parsing, ChannelContext& channel_context);

    /**
     * Apply shared prefix fallback defaults used by install/update entry points.
     */
    void configure_common_prefix_fallbacks(Configuration& config, bool create_base);

    /**
     * Validate target prefix preconditions and warn when no channels are configured.
     */
    void validate_target_prefix_and_channels(const Context& ctx, bool create_env);

    /**
     * Prepare solver state: channels, package cache, database, and root package loading.
     * Computes ``python_minor_version_for_prefilter`` for sharded repodata: explicit python from
     * specs, implicit fallback on the first solve attempt, or the installed prefix minor on retry
     * when no explicit python is given. When ``no_py_pin`` is true (``--no-py-pin``), no python
     * minor is used for shard prefiltering.
     */
    std::pair<solver::libsolv::Database, MultiPackageCache> prepare_solver_context(
        Context& ctx,
        ChannelContext& channel_context,
        const std::vector<std::string>& raw_specs,
        bool is_retry,
        bool no_py_pin
    );

    /**
     * Load target prefix metadata and register installed packages in the solver database.
     */
    PrefixData load_prefix_data_and_installed(
        Context& ctx,
        ChannelContext& channel_context,
        solver::libsolv::Database& db
    );

    /**
     * Handle unsatisfiable outcomes, including optional one-shot retry with clean cache policy.
     *
     * Returns ``true`` if a retry callback was triggered and the caller should return early.
     */
    bool handle_unsolvable_with_retry(
        solver::libsolv::Solver::Outcome& outcome,
        const Palette& palette,
        bool json_output,
        bool retry_clean_cache,
        bool is_retry,
        std::size_t& local_repodata_ttl,
        solver::libsolv::Database& db,
        const std::function<void()>& retry_fn,
        const std::function<void()>& pre_throw_hint = {}
    );

    /**
     * Emit unsatisfiable diagnostics as JSON when JSON output mode is enabled.
     */
    void write_unsolvable_json_if_needed(
        bool json_output,
        solver::libsolv::Database& db,
        solver::libsolv::UnSolvable& unsolvable
    );

    /**
     * Execute a transaction with optional hooks and return whether it was accepted.
     */
    bool execute_transaction(
        MTransaction& transaction,
        Context& ctx,
        ChannelContext& channel_context,
        PrefixData& prefix_data,
        const std::function<void()>& before_execute = {},
        const std::function<void()>& after_execute = {},
        const std::function<void()>& on_abort = {}
    );

    /**
     * Build a transaction from a solved outcome.
     *
     * The database is passed by value and moved into ``MTransaction`` so solver memory can
     * be released before transaction execution starts. This is used by both install/update
     * flows to reduce peak memory usage during package download/extraction and ``.pyc`` creation.
     */
    MTransaction make_transaction_from_solution(
        Context& ctx,
        solver::libsolv::Database db,
        const solver::Request& request,
        const solver::libsolv::Solver::Outcome& outcome,
        MultiPackageCache& package_caches
    );

    /**
     * Execute secondary package manager operations (e.g. pip/uv) for recorded specs.
     */
    void execute_other_pkg_managers(
        const std::vector<detail::other_pkg_mgr_spec>& other_specs,
        const Context& ctx,
        pip::Update update
    );

    /**
     * Conditionally execute secondary package manager operations after transaction evaluation.
     */
    void execute_other_pkg_managers_if_needed(
        bool transaction_accepted,
        bool dry_run,
        const std::vector<detail::other_pkg_mgr_spec>& other_specs,
        const Context& ctx,
        pip::Update update
    );

    /**
     * Extract an explicit python minor requirement (e.g. "3.12") from specs.
     *
     * Parses each ``python`` ``MatchSpec``, applies ``relax_version_spec_to_minor`` to the
     * version, and returns the version if it is a single ``==`` equality (e.g. full pins relax to
     * ``major.minor``). Skips specs that do not parse or do not yield such an equality after
     * relaxation.
     */
    std::optional<specs::Version>
    extract_requested_python_minor(const std::vector<std::string>& specs);

}

#endif  // MAMBA_UTILS_HPP
