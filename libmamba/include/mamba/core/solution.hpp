// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SOLUTION_HPP
#define MAMBA_CORE_SOLUTION_HPP

#include <variant>
#include <vector>

#include "package_info.hpp"

namespace mamba
{
    class Solution
    {
    public:

        struct Upgrade
        {
            PackageInfo remove;
            PackageInfo install;
        };
        struct Downgrade
        {
            PackageInfo remove;
            PackageInfo install;
        };
        struct Change
        {
            PackageInfo remove;
            PackageInfo install;
        };
        struct Reinstall
        {
            PackageInfo what;
        };
        struct Remove
        {
            PackageInfo remove;
        };
        struct Install
        {
            PackageInfo install;
        };
        using Action = std::variant<Upgrade, Downgrade, Change, Reinstall, Remove, Install>;
        using action_list = std::vector<Action>;

        using size_type = typename action_list::size_type;

        Solution() = default;
        Solution(const action_list& actions);
        Solution(action_list&& actions);

        [[nodiscard]] auto actions() const -> const action_list&;
        [[nodiscard]] auto actions() -> action_list&;

        template <typename UnaryFunc>
        void for_each_to_install(UnaryFunc&& func);
        template <typename UnaryFunc>
        void for_each_to_install(UnaryFunc&& func) const;
        template <typename UnaryFunc>
        void for_each_to_remove(UnaryFunc&& func);
        template <typename UnaryFunc>
        void for_each_to_remove(UnaryFunc&& func) const;

    private:

        action_list m_actions = {};
    };
}

#include <type_traits>

namespace mamba
{
    /********************************
     *  Implementation of Solution  *
     ********************************/

    namespace detail
    {
        template <typename T, typename... U>
        inline constexpr bool is_any_of_v = std::disjunction_v<std::is_same<T, U>...>;

        template <typename T>
        inline constexpr bool has_remove_v = is_any_of_v<
            T,
            Solution::Upgrade,
            Solution::Downgrade,
            Solution::Change,
            Solution::Remove>;

        template <typename Action>
        auto to_remove_ptr(Action& action)
        {
            using PackageInfoPtr = std::conditional_t<std::is_const_v<Action>, const PackageInfo*, PackageInfo*>;
            return std::visit(
                [](auto& a) -> PackageInfoPtr
                {
                    using A = std::decay_t<decltype(a)>;
                    if constexpr (has_remove_v<A>)
                    {
                        return &(a.remove);
                    }
                    else if constexpr (std::is_same_v<A, Solution::Reinstall>)
                    {
                        return &(a.what);
                    }
                    return nullptr;
                },
                action
            );
        }

        /** Mimick C++23 deducing this. */
        template <typename Self, typename UnaryFunc>
        void for_each_to_remove_impl(Self&& self, UnaryFunc&& func)
        {
            for (auto& a : self.actions())
            {
                if (auto* const ptr = to_remove_ptr(a))
                {
                    func(*ptr);
                }
            }
        }

        template <typename T>
        inline constexpr bool has_install_v = is_any_of_v<
            T,
            Solution::Upgrade,
            Solution::Downgrade,
            Solution::Change,
            Solution::Install>;

        template <typename Action>
        auto to_install_ptr(Action& action)
        {
            using PackageInfoPtr = std::conditional_t<std::is_const_v<Action>, const PackageInfo*, PackageInfo*>;
            return std::visit(
                [](auto& a) -> PackageInfoPtr
                {
                    using A = std::decay_t<decltype(a)>;
                    if constexpr (has_install_v<A>)
                    {
                        return &(a.install);
                    }
                    else if constexpr (std::is_same_v<A, Solution::Reinstall>)
                    {
                        return &(a.what);
                    }
                    return nullptr;
                },
                action
            );
        }

        /** Mimick C++23 deducing this. */
        template <typename Self, typename UnaryFunc>
        void for_each_to_install_impl(Self&& self, UnaryFunc&& func)
        {
            for (auto& a : self.actions())
            {
                if (auto* const ptr = to_install_ptr(a))
                {
                    func(*ptr);
                }
            }
        }
    }

    template <typename UnaryFunc>
    void Solution::for_each_to_install(UnaryFunc&& func)
    {
        detail::for_each_to_install_impl(*this, std::forward<UnaryFunc>(func));
    }

    template <typename UnaryFunc>
    void Solution::for_each_to_install(UnaryFunc&& func) const
    {
        detail::for_each_to_install_impl(*this, std::forward<UnaryFunc>(func));
    }

    template <typename UnaryFunc>
    void Solution::for_each_to_remove(UnaryFunc&& func)
    {
        detail::for_each_to_remove_impl(*this, std::forward<UnaryFunc>(func));
    }

    template <typename UnaryFunc>
    void Solution::for_each_to_remove(UnaryFunc&& func) const
    {
        detail::for_each_to_remove_impl(*this, std::forward<UnaryFunc>(func));
    }
}
#endif
