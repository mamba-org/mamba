// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <string_view>
#include <tuple>

#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/build.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/repo.hpp"

#include "solver/libsolv/helpers.hpp"

namespace mamba
{
    namespace
    {
        // Keeping the solv-cpp header private
        auto srepo(const MRepo& r) -> solv::ObjRepoViewConst
        {
            return solv::ObjRepoViewConst{ *const_cast<const ::Repo*>(r.repo()) };
        }

        auto srepo(MRepo& r) -> solv::ObjRepoView
        {
            return solv::ObjRepoView{ *r.repo() };
        }
    }

    MRepo::MRepo(::Repo* repo)
        : m_repo(repo)
    {
    }

    MRepo::MRepo(
        MPool& pool,
        const std::string_view name,
        const std::vector<specs::PackageInfo>& package_infos,
        PipAsPythonDependency add
    )
    {
        auto [_, repo] = pool.pool().add_repo(name);
        m_repo = repo.raw();
        for (auto& info : package_infos)
        {
            LOG_INFO << "Adding package record to repo " << info.name;
            auto [id, solv] = srepo(*this).add_solvable();
            solver::libsolv::set_solvable(pool.pool(), solv, info);
        }
        if (add == PipAsPythonDependency::Yes)
        {
            solver::libsolv::add_pip_as_python_dependency(pool.pool(), repo);
        }
        repo.internalize();
    }

    void MRepo::set_priority(int priority, int subpriority)
    {
        m_repo->priority = priority;
        m_repo->subpriority = subpriority;
    }

    auto MRepo::name() const -> std::string_view
    {
        return srepo(*this).name();
    }

    auto MRepo::py_priority() const -> std::tuple<int, int>
    {
        return std::make_tuple(m_repo->priority, m_repo->subpriority);
    }

    auto MRepo::package_count() const -> std::size_t
    {
        return srepo(*this).solvable_count();
    }

    Id MRepo::id() const
    {
        return srepo(*this).id();
    }

    Repo* MRepo::repo() const
    {
        return m_repo;
    }
}  // namespace mamba
