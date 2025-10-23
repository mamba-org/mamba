// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_ENVIRONMENT_LOCKFILE_IMPL_HPP
#define MAMBA_CORE_ENVIRONMENT_LOCKFILE_IMPL_HPP

namespace mamba
{

    tl::expected<EnvironmentLockFile, mamba_error>
    read_conda_environment_lockfile(const fs::u8path& lockfile_location);

    tl::expected<EnvironmentLockFile, mamba_error>
    read_mambajs_environment_lockfile(const fs::u8path& lockfile_location);





}
#endif