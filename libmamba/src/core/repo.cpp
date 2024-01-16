// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string_view>
#include <type_traits>

#include "mamba/core/repo.hpp"
#include "solv-cpp/repo.hpp"

namespace mamba
{
    namespace
    {
        // Keeping the solv-cpp header private
        auto srepo(const MRepo& r) -> solv::ObjRepoViewConst
        {
            return solv::ObjRepoViewConst{ *const_cast<const ::Repo*>(r.repo()) };
        }
    }

    MRepo::MRepo(::Repo* repo)
        : m_repo(repo)
    {
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

    auto MRepo::priority() const -> Priorities
    {
        return { /* .priority= */ m_repo->priority, /* .subpriority= */ m_repo->subpriority };
    }

    auto MRepo::package_count() const -> std::size_t
    {
        return srepo(*this).solvable_count();
    }

    auto MRepo::id() const -> RepoId
    {
        static_assert(std::is_same_v<RepoId, ::Id>);
        return srepo(*this).id();
    }

    auto MRepo::repo() const -> ::Repo*
    {
        return m_repo;
    }
}  // namespace mamba
