// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SHARD_PYTHON_MINOR_PREFILTER_HPP
#define MAMBA_CORE_SHARD_PYTHON_MINOR_PREFILTER_HPP

#include <optional>
#include <string>

#include "mamba/specs/version.hpp"
#include "mamba/specs/version_spec.hpp"

namespace mamba
{
    /**
     * If ``vs`` is a single ``==…`` leaf, return the parsed ``Version``; otherwise ``nullopt``.
     */
    [[nodiscard]] auto version_from_single_equality_spec(const specs::VersionSpec& vs)
        -> std::optional<specs::Version>;

    /**
     * For a single ``== <full version>`` leaf, replace with ``== <major.minor>`` so
     * ``VersionSpec::contains`` matches a user ``python=X.Y`` point. Other specs are returned
     * unchanged.
     */
    [[nodiscard]] auto relax_version_spec_to_minor(const specs::VersionSpec& vs)
        -> specs::VersionSpec;

    /**
     * Whether a ``depends`` line for ``python`` is compatible with
     * ``python_minor_version_for_prefilter``. Uses ``VersionSpec::contains`` on the parsed version
     * first; if that fails, relaxes exact on ``major.minor`` (see ``relax_version_spec_to_minor``)
     * and tests again. Non-python dependencies always return true; parse failures return true (no
     * prefilter).
     */
    [[nodiscard]] auto dependency_matches_python_minor_version_for_prefilter(
        const std::string& dependency_spec,
        const specs::Version& python_minor_version_for_prefilter
    ) -> bool;
}

#endif
