// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_EXCLUDE_NEWER_HPP
#define MAMBA_CORE_EXCLUDE_NEWER_HPP

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace mamba
{
    /**
     * Resolved per-package ``exclude_newer`` cutoffs.
     *
     * Cutoffs are stored as Unix epoch seconds (``std::uint64_t``) for compatibility with
     * conda repodata timestamps and ``Database::Settings``. Parsing uses
     * ``std::chrono::sys_seconds`` internally; see ``resolve_exclude_newer_cutoff``.
     *
     * When a package name is present:
     * - ``std::nullopt`` exempts the package from the global policy (``false`` in config)
     * - a timestamp value applies a package-specific cutoff
     *
     * Packages not listed fall back to the global cutoff.
     */
    using ExcludeNewerPackageCutoffs = std::unordered_map<std::string, std::optional<std::uint64_t>>;

    /**
     * Resolved ``exclude_newer`` policy used by the solver / database.
     *
     * Holds Unix-second cutoffs only. Raw configuration strings live on ``Context`` and are
     * resolved via ``resolve_exclude_newer_policy``.
     *
     * When set, packages with a policy timestamp newer than the effective cutoff are excluded
     * during repodata loading. For repodata JSON, ``indexed_timestamp`` is preferred over
     * ``timestamp`` when both are present, matching conda's ``--exclude-newer`` semantics.
     * Per-package overrides take precedence over the global cutoff; a package mapped to
     * ``std::nullopt`` is exempt (``false`` in config). The ``.solv`` cache path is not
     * filtered; invalidate the cache when this policy changes.
     *
     * Background and cross-ecosystem tracking:
     * https://github.com/conda/conda/issues/15759
     */
    struct ExcludeNewerPolicy
    {
        /**
         * Resolved global cutoff timestamp in seconds.
         */
        std::optional<std::uint64_t> global = std::nullopt;

        /**
         * Resolved per-package timestamp cutoffs.
         */
        ExcludeNewerPackageCutoffs per_package = {};

        /** Return whether no cutoff is configured. */
        [[nodiscard]] auto empty() const -> bool
        {
            return !global.has_value() && per_package.empty();
        }

        /**
         * Return the effective cutoff for ``package_name``.
         *
         * Per-package entries take precedence over the global cutoff. A mapped ``std::nullopt``
         * means the package is exempt.
         */
        [[nodiscard]] auto cutoff_for(std::string_view package_name) const
            -> std::optional<std::uint64_t>;

        /**
         * Return whether ``pkg_timestamp`` is newer than the effective cutoff for ``package_name``.
         *
         * Exempt packages (no cutoff) are never excluded.
         */
        [[nodiscard]] auto
        excludes(std::string_view package_name, std::uint64_t pkg_timestamp) const -> bool;
    };

    /**
     * Resolve raw per-package ``exclude_newer`` configuration values.
     *
     * @param exclude_newer_package Map of package name to config value (duration, date, or
     * ``false``).
     * @param now_seconds Reference time for relative durations, in Unix seconds.
     *
     * @throws mamba_error when a non-``false`` value cannot be parsed.
     */
    [[nodiscard]] auto resolve_exclude_newer_package_cutoffs(
        const std::map<std::string, std::string>& exclude_newer_package,
        std::uint64_t now_seconds
    ) -> ExcludeNewerPackageCutoffs;

    /**
     * Resolve raw ``exclude_newer`` configuration into a policy for the database.
     *
     * @param exclude_newer Global cutoff configuration string.
     * @param exclude_newer_package Per-package overrides.
     * @param now_seconds Reference time for relative durations, in Unix seconds.
     *
     * @throws mamba_error when a value cannot be parsed.
     */
    [[nodiscard]] auto resolve_exclude_newer_policy(
        std::string_view exclude_newer,
        const std::map<std::string, std::string>& exclude_newer_package,
        std::uint64_t now_seconds
    ) -> ExcludeNewerPolicy;

    /**
     * Resolve a global ``exclude_newer`` configuration value to an absolute Unix
     * timestamp cutoff in seconds.
     *
     * The public API exposes ``std::uint64_t`` seconds for compatibility with repodata
     * timestamps. Internally, date and datetime values are parsed with
     * ``std::chrono::parse`` when the standard library provides it, otherwise with
     * Howard Hinnant's ``date`` library until libc++ implements P0355
     * (https://github.com/llvm/llvm-project/issues/166051). Duration strings
     * (``7d``, ``P7D``, plain seconds) use custom parsers because the standard library
     * does not provide ISO 8601 duration parsing.
     *
     * Matches conda's ``exclude_newer`` semantics:
     * - Durations (``7d``, ``P7D``, plain seconds) resolve to ``now - duration``
     * - Date-only values (``YYYY-MM-DD``) resolve to the start of the next UTC day
     * - Datetimes resolve to the given instant (naive values are UTC)
     * - Zero durations (``0``, ``0d``, ``P0D``) resolve to ``now``
     *
     * @param value Raw configuration string.
     * @param now_seconds Reference time for relative durations, in Unix seconds.
     * @param package_name When resolving a per-package override, used in parse-failure warnings.
     *
     * Returns ``std::nullopt`` when ``value`` is empty/whitespace-only.
     *
     * @throws mamba_error when the value cannot be parsed.
     */
    [[nodiscard]] auto resolve_exclude_newer_cutoff(
        std::string_view value,
        std::uint64_t now_seconds,
        std::string_view package_name = {}
    ) -> std::optional<std::uint64_t>;

    namespace detail
    {
        /**
         * Parse an ISO 8601 duration (``P…Y…M…W…DT…H…M…S``) to seconds.
         *
         * Returns ``std::nullopt`` when ``value`` is not an ISO 8601 duration.
         *
         * @throws mamba_error when the value starts with ``P`` but has no components.
         */
        [[nodiscard]] auto parse_iso8601_duration_seconds(std::string_view value)
            -> std::optional<std::chrono::seconds>;

        /**
         * Parse a compact duration (``(n)y(n)M(n)w(n)d(n)h(n)m(n)s``, e.g. ``7d``, ``3d12h``)
         * to seconds.
         *
         * Returns ``std::nullopt`` when ``value`` is not a compact duration.
         */
        [[nodiscard]] auto parse_compact_duration_seconds(std::string_view value)
            -> std::optional<std::chrono::seconds>;
    }

}  // namespace mamba

#include "mamba/core/detail/chrono_parse.hpp"

#endif
