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
    namespace detail
    {
        template <typename T, typename... U>
        inline constexpr bool is_any_of_v = std::disjunction_v<std::is_same<T, U>...>;
    }

    namespace action
    {
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

        template <typename T>
        inline constexpr bool has_remove_v = detail::is_any_of_v<T, Upgrade, Downgrade, Change, Remove>;

        template <typename T>
        inline constexpr bool has_install_v = detail::is_any_of_v<T, Upgrade, Downgrade, Change, Install>;

        using Action = std::variant<Upgrade, Downgrade, Change, Reinstall, Remove, Install>;
    }

    using Solution = std::vector<action::Action>;

    template <typename Iter, typename UnaryFunc>
    void for_each_to_remove(Iter first, Iter last, UnaryFunc&& func);

    template <typename Range, typename UnaryFunc>
    void for_each_to_remove(Range&& solution, UnaryFunc&& func);

    template <typename Iter, typename UnaryFunc>
    void for_each_to_install(Iter first, Iter last, UnaryFunc&& func);

    template <typename Range, typename UnaryFunc>
    void for_each_to_install(Range&& solution, UnaryFunc&& func);

}

#include <type_traits>

namespace mamba
{
    /********************************
     *  Implementation of Solution  *
     ********************************/

    namespace action::detail
    {
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
                    else if constexpr (std::is_same_v<A, Reinstall>)
                    {
                        return &(a.what);
                    }
                    return nullptr;
                },
                action
            );
        }
    }

    template <typename Iter, typename UnaryFunc>
    void for_each_to_remove(Iter first, Iter last, UnaryFunc&& func)
    {
        for (; first != last; ++first)
        {
            if (auto* const ptr = action::detail::to_remove_ptr(*first))
            {
                func(*ptr);
            }
        }
    }

    template <typename Range, typename UnaryFunc>
    void for_each_to_remove(Range&& self, UnaryFunc&& func)
    {
        return for_each_to_remove(self.begin(), self.end(), std::forward<UnaryFunc>(func));
    }

    namespace action::detail
    {
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
                    else if constexpr (std::is_same_v<A, Reinstall>)
                    {
                        return &(a.what);
                    }
                    return nullptr;
                },
                action
            );
        }
    }

    template <typename Iter, typename UnaryFunc>
    void for_each_to_install(Iter first, Iter last, UnaryFunc&& func)
    {
        for (; first != last; ++first)
        {
            if (auto* const ptr = action::detail::to_install_ptr(*first))
            {
                func(*ptr);
            }
        }
    }

    template <typename Self, typename UnaryFunc>
    void for_each_to_install(Self&& self, UnaryFunc&& func)
    {
        return for_each_to_install(self.begin(), self.end(), std::forward<UnaryFunc>(func));
    }
}
#endif
