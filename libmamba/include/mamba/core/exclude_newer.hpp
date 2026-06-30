// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_EXCLUDE_NEWER_HPP
#define MAMBA_CORE_EXCLUDE_NEWER_HPP

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace mamba
{
    namespace detail
    {
        struct ExcludeNewerPackageHash
        {
            [[nodiscard]] auto operator()(std::string_view value) const noexcept -> std::size_t
            {
                return std::hash<std::string_view>{}(value);
            }
        };

        struct ExcludeNewerPackageEqual
        {
            [[nodiscard]] auto operator()(std::string_view lhs, std::string_view rhs) const noexcept
                -> bool
            {
                return lhs == rhs;
            }
        };
    }  // namespace detail

    /**
     * Resolved per-package ``exclude_newer`` cutoffs.
     *
     * When a package name is present:
     * - ``std::nullopt`` exempts the package from the global policy (``false`` in config)
     * - a timestamp value applies a package-specific cutoff
     *
     * Packages not listed fall back to the global cutoff.
     */
    using ExcludeNewerPackageCutoffs = std::unordered_map<
        std::string,
        std::optional<std::uint64_t>,
        detail::ExcludeNewerPackageHash,
        detail::ExcludeNewerPackageEqual>;

    struct ExcludeNewerPolicy
    {
        /**
         * Raw ``exclude_newer`` configuration values from CLI or configuration file.
         *
         * Background and cross-ecosystem tracking:
         * https://github.com/conda/conda/issues/15759
         */
        std::string exclude_newer;
        std::map<std::string, std::string> exclude_newer_package;

        /**
         * Resolved global cutoff timestamp in seconds.
         */
        std::optional<std::uint64_t> global = std::nullopt;

        /**
         * Resolved per-package timestamp cutoffs.
         */
        ExcludeNewerPackageCutoffs per_package = {};

        [[nodiscard]] auto empty() const -> bool
        {
            return exclude_newer.empty() && exclude_newer_package.empty();
        }

        [[nodiscard]] auto cutoff_for(std::string_view package_name) const
            -> std::optional<std::uint64_t>;

        [[nodiscard]] auto
        excludes(std::string_view package_name, std::uint64_t pkg_timestamp) const -> bool;
    };

    /**
     * Resolve raw per-package ``exclude_newer`` configuration values.
     *
     * @throws std::invalid_argument when a non-``false`` value cannot be parsed.
     */
    [[nodiscard]] auto resolve_exclude_newer_package_cutoffs(
        const std::map<std::string, std::string>& exclude_newer_package,
        std::uint64_t now_seconds
    ) -> ExcludeNewerPackageCutoffs;

    /**
     * Resolve a global ``exclude_newer`` configuration value to an absolute Unix
     * timestamp cutoff in seconds.
     *
     * Matches conda's ``exclude_newer`` semantics:
     * - Durations (``7d``, ``P7D``, plain seconds) resolve to ``now - duration``
     * - Date-only values (``YYYY-MM-DD``) resolve to the start of the next UTC day
     * - Datetimes resolve to the given instant (naive values are UTC)
     * - Zero durations (``0``, ``0d``, ``P0D``) resolve to ``now``
     *
     * Returns ``std::nullopt`` when ``value`` is empty/whitespace-only.
     *
     * @throws std::invalid_argument when the value cannot be parsed.
     */
    [[nodiscard]] auto resolve_exclude_newer_cutoff(std::string_view value, std::uint64_t now_seconds)
        -> std::optional<std::uint64_t>;

}  // namespace mamba

#endif
