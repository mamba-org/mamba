// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/solution.hpp"

namespace mamba
{
    Solution::Solution(const action_list& actions)
        : m_actions(actions)
    {
    }

    Solution::Solution(action_list&& actions)
        : m_actions(std::move(actions))
    {
    }

    auto Solution::empty() const -> bool
    {
        return actions().empty();
    }

    auto Solution::size() const -> size_type
    {
        return actions().size();
    }

    auto Solution::actions() const -> const action_list&
    {
        return m_actions;
    }

    auto Solution::actions() -> action_list&
    {
        return m_actions;
    }
}
