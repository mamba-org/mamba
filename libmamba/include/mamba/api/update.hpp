// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_UPDATE_HPP
#define MAMBA_API_UPDATE_HPP

namespace mamba
{
    class Configuration;

    enum class UpdateAll : bool
    {
        No = false,
        Yes = true,
    };

    enum class PruneDeps : bool
    {
        No = false,
        Yes = true,
    };

    enum class EnvUpdate : bool  // Specific to `env update` command
    {
        No = false,
        Yes = true,
    };

    enum class RemoveNotSpecified : bool  // Specific to `env update` command
    {
        No = false,
        Yes = true,
    };

    struct UpdateParams
    {
        UpdateAll update_all = UpdateAll::No;
        PruneDeps prune_deps = PruneDeps::No;
        EnvUpdate env_update = EnvUpdate::No;
        RemoveNotSpecified remove_not_specified = RemoveNotSpecified::No;
    };

    void update(Configuration& config, const UpdateParams& update_params = {});
}

#endif
