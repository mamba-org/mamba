// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/repo_checker_store.hpp"

namespace mamba
{

    RepoCheckerStore::RepoCheckerStore(const Context& ctx)
    {
    }

    auto RepoCheckerStore::find_checker(const Channel& chan) const -> const RepoChecker*
    {
        for (auto& [candidate_chan, checker] : m_repo_checkers)
        {
            if (candidate_chan.contains_equivalent(chan))
            {
                return &checker;
            }
        }
        return nullptr;
    }

    auto RepoCheckerStore::contains_checker(const Channel& chan) const -> bool
    {
        return find_checker(chan) != nullptr;
    }

    auto RepoCheckerStore::at_checker(const Channel& chan) const -> const RepoChecker&
    {
        if (auto ptr = find_checker(chan))
        {
            return *ptr;
        }
        throw std::range_error("Checker not found");
    }
}
