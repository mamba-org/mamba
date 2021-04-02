// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_INSTALL_HPP
#define MAMBA_API_INSTALL_HPP

#include "mamba/core/context.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/solver.hpp"

#include <string>
#include <vector>


namespace mamba
{
    void install();

    void install_specs(const std::vector<std::string>& specs,
                       bool create_env = false,
                       int solver_flag = SOLVER_INSTALL,
                       int is_retry = 0);

    void install_explicit_specs(const std::vector<std::string>& specs);

    namespace detail
    {
        void file_specs_hook(std::vector<std::string>& file_specs);

        MRepo create_repo_from_pkgs_dir(MPool& pool, const fs::path& pkgs_dir);

        bool download_explicit(const std::vector<PackageInfo>& pkgs);
    }
}

#endif
