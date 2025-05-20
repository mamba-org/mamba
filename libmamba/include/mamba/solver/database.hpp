// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLVER_DATABASE_HPP
#define MAMBA_SOLVER_DATABASE_HPP

#include <string>
#include <vector>

#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/specs/package_info.hpp"

namespace mamba::solver
{
    class Database
    {
    public:

        virtual ~Database() = default;

        virtual void add_repo_from_repodata_json(
            const fs::u8path& filename,
            const std::string& repo_url,
            const std::string& channel_id,
            bool verify_artifacts = false
        ) = 0;

        virtual void add_repo_from_packages(
            const std::vector<specs::PackageInfo>& packages,
            const std::string& repo_name,
            bool pip_as_python_dependency = false
        ) = 0;

        virtual void set_installed_repo(const std::string& repo_name) = 0;

        virtual bool has_package(const specs::MatchSpec& spec) = 0;
    };

    inline auto database_has_package(DatabaseVariant& database, const specs::MatchSpec& spec) -> bool
    {
        if (auto* libsolv_db = std::get_if<libsolv::Database>(&database))
        {
            return libsolv_db->has_package(spec);
        }
        else if (auto* resolvo_db = std::get_if<resolvo::Database>(&database))
        {
            return resolvo_db->has_package(spec);
        }
        throw std::runtime_error("Invalid database variant");
    }
}

#endif  // MAMBA_SOLVER_DATABASE_HPP
