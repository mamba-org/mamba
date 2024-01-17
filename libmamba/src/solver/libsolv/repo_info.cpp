// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string_view>
#include <type_traits>

#include "mamba/solver/libsolv/repo_info.hpp"
#include "solv-cpp/repo.hpp"

namespace mamba::solver::libsolv
{
    namespace
    {
        // Keeping the solv-cpp header private
        auto srepo(const RepoInfo& r) -> solv::ObjRepoViewConst
        {
            return solv::ObjRepoViewConst{ *const_cast<const ::Repo*>(r.repo()) };
        }
    }

    RepoInfo::RepoInfo(::Repo* repo)
        : m_ptr(repo)
    {
    }

    auto RepoInfo::name() const -> std::string_view
    {
        return srepo(*this).name();
    }

    auto RepoInfo::priority() const -> Priorities
    {
        static_assert(std::is_same_v<decltype(m_ptr->priority), Priorities::value_type>);
        return { /* .priority= */ m_ptr->priority, /* .subpriority= */ m_ptr->subpriority };
    }

    auto RepoInfo::package_count() const -> std::size_t
    {
        return srepo(*this).solvable_count();
    }

    auto RepoInfo::id() const -> RepoId
    {
        static_assert(std::is_same_v<RepoId, ::Id>);
        return srepo(*this).id();
    }

    auto RepoInfo::repo() const -> ::Repo*
    {
        return m_ptr;
    }

    auto operator==(RepoInfo lhs, RepoInfo rhs) -> bool
    {
        return lhs.m_ptr == rhs.m_ptr;
    }

    auto operator!=(RepoInfo lhs, RepoInfo rhs) -> bool
    {
        return !(rhs == lhs);
    }
}  // namespace mamba
