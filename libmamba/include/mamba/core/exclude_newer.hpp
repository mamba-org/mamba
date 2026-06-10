// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_EXCLUDE_NEWER_HPP
#define MAMBA_CORE_EXCLUDE_NEWER_HPP

#include <cstdint>
#include <optional>
#include <string_view>

namespace mamba
{
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
