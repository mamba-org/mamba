// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_REPO_CHECKER_STORE_HPP
#define MAMBA_CORE_REPO_CHECKER_STORE_HPP

#include <utility>
#include <vector>

#include "mamba/specs/channel.hpp"
#include "mamba/validation/repo_checker.hpp"

namespace mamba
{
    class Context;
    class ChannelContext;
    class MultiPackageCache;

    class RepoCheckerStore
    {
    public:

        using Channel = specs::Channel;
        using RepoChecker = validation::RepoChecker;
        using repo_checker_list = std::vector<std::pair<Channel, RepoChecker>>;

        [[nodiscard]] static auto
        make(const Context& ctx, ChannelContext& cc, MultiPackageCache& caches) -> RepoCheckerStore;

        explicit RepoCheckerStore(repo_checker_list checkers);

        [[nodiscard]] auto find_checker(const Channel& chan) -> RepoChecker*;

        [[nodiscard]] auto contains_checker(const Channel& chan) -> bool;

        [[nodiscard]] auto at_checker(const Channel& chan) -> RepoChecker&;

    private:

        repo_checker_list m_repo_checkers = {};
    };
}
#endif
