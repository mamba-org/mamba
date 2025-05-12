// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_PACKAGE_DATABASE_LOADER_HPP
#define MAMBA_CORE_PACKAGE_DATABASE_LOADER_HPP

#include "mamba/core/error_handling.hpp"
#include "mamba/solver/libsolv/repo_info.hpp"
#include "mamba/specs/channel.hpp"

namespace mamba
{
    class Context;
    class PrefixData;
    class SubdirIndexLoader;

    namespace solver::libsolv
    {
        class Database;
    }

    void add_spdlog_logger_to_database(solver::libsolv::Database& database);

    auto load_subdir_in_database(  //
        const Context& ctx,
        solver::libsolv::Database& database,
        const SubdirIndexLoader& subdir
    ) -> expected_t<solver::libsolv::RepoInfo>;

    auto load_installed_packages_in_database(
        const Context& ctx,
        solver::libsolv::Database& database,
        const PrefixData& prefix
    ) -> solver::libsolv::RepoInfo;
}
#endif
