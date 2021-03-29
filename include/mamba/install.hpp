// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_INSTALL_HPP
#define MAMBA_INSTALL_HPP

#include "mamba/context.hpp"
#include "mamba/mamba_fs.hpp"
#include "mamba/package_info.hpp"
#include "mamba/pool.hpp"
#include "mamba/repo.hpp"
#include "mamba/solver.hpp"

#include <string>
#include <vector>


namespace mamba
{
    void install(const std::vector<std::string>& specs = {}, const fs::path& prefix = "");

    namespace detail
    {
        void install_specs(const std::vector<std::string>& specs,
                           bool create_env = false,
                           int solver_flag = SOLVER_INSTALL,
                           int is_retry = 0);

        void file_specs_hook(std::vector<std::string>& file_specs);

        MRepo create_repo_from_pkgs_dir(MPool& pool, const fs::path& pkgs_dir);

        void install_explicit_specs(std::vector<std::string>& specs);

        bool download_explicit(const std::vector<PackageInfo>& pkgs);
    }
}

#endif
