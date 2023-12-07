// Copyright (c) 2023, QuantStack and Mamba Contributors
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

    class RepoCheckerStore
    {
    public:

        using Channel = specs::Channel;
        using RepoChecker = validation::RepoChecker;

        explicit RepoCheckerStore(const Context& ctx);

        [[nodiscard]] auto find_checker(const Channel& chan) const -> const RepoChecker*;

        [[nodiscard]] auto contains_checker(const Channel& chan) const -> bool;

        [[nodiscard]] auto at_checker(const Channel& chan) const -> const RepoChecker&;

    private:

        using repo_checker_list = std::vector<std::pair<Channel, RepoChecker>>;

        repo_checker_list m_repo_checkers = {};
    };
}
#endif
