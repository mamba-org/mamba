// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SHARD_PYTHON_MINOR_PREFILTER_HPP
#define MAMBA_CORE_SHARD_PYTHON_MINOR_PREFILTER_HPP

#include <string>

#include "mamba/specs/version.hpp"
#include "mamba/specs/version_spec.hpp"

namespace mamba
{
    /**
     * For a single ``== <full version>`` leaf, replace with ``== <major.minor>`` so
     * ``VersionSpec::contains`` matches a user ``python=X.Y`` point. Other specs are returned
     * unchanged.
     */
    [[nodiscard]] auto relax_version_spec_to_minor(const specs::VersionSpec& vs)
        -> specs::VersionSpec;

    /**
     * Whether a ``depends`` line for ``python`` is compatible with the requested minor.
     * Uses ``VersionSpec::contains`` on the parsed version first; if that fails, relaxes exact
     * on ``major.minor`` (see ``relax_version_spec_to_minor``) and tests again.
     * Non-python dependencies always return true; parse failures return true (no prefilter).
     */
    [[nodiscard]] auto dependency_matches_requested_python_minor(
        const std::string& dependency_spec,
        const specs::Version& requested_python_minor
    ) -> bool;
}

#endif
