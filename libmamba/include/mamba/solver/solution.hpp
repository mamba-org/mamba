// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SOLUTION_HPP
#define MAMBA_CORE_SOLUTION_HPP

#include <ranges>
#include <type_traits>
#include <variant>
#include <vector>

#include "mamba/specs/package_info.hpp"
#include "mamba/util/type_traits.hpp"

namespace mamba::solver
{
    struct Solution
    {
        struct Omit
        {
            specs::PackageInfo what;
        };

        struct Upgrade
        {
            specs::PackageInfo remove;
            specs::PackageInfo install;
        };

        struct Downgrade
        {
            specs::PackageInfo remove;
            specs::PackageInfo install;
        };

        struct Change
        {
            specs::PackageInfo remove;
            specs::PackageInfo install;
        };

        struct Reinstall
        {
            specs::PackageInfo what;
        };

        struct Remove
        {
            specs::PackageInfo remove;
        };

        struct Install
        {
            specs::PackageInfo install;
        };

        template <typename T>
        inline static constexpr bool has_remove_v = util::is_any_of_v<T, Upgrade, Downgrade, Change, Remove>;

        template <typename T>
        inline static constexpr bool has_install_v = util::is_any_of_v<T, Upgrade, Downgrade, Change, Install>;

        using Action = std::variant<Omit, Upgrade, Downgrade, Change, Reinstall, Remove, Install>;
        using action_list = std::vector<Action>;

        action_list actions = {};

        /**
         * Return a view of all unique packages involved in the solution.
         *
         * The view is invalidated if @ref actions is modified.
         */
        [[nodiscard]] auto packages() const;
        [[nodiscard]] auto packages();

        /**
         * Return a view of all packages that need to be removed.
         *
         * The view is invalidated if @ref actions is modified.
         */
        [[nodiscard]] auto packages_to_remove() const;
        [[nodiscard]] auto packages_to_remove();

        /**
         * Return a view of all packages that need to be installed.
         *
         * The view is invalidated if @ref actions is modified.
         */
        [[nodiscard]] auto packages_to_install() const;
        [[nodiscard]] auto packages_to_install();

        /**
         * Return a view of all packages that are omitted.
         *
         * The view is invalidated if @ref actions is modified.
         */
        [[nodiscard]] auto packages_to_omit() const;
        [[nodiscard]] auto packages_to_omit();
    };

    /********************************
     *  Implementation of Solution  *
     ********************************/

    namespace detail
    {
        template <typename Action>
        auto to_remove_ptr(Action& action)
        {
            using PackageInfoPtr = std::
                conditional_t<std::is_const_v<Action>, const specs::PackageInfo*, specs::PackageInfo*>;
            return std::visit(
                [](auto& a) -> PackageInfoPtr
                {
                    using A = std::decay_t<decltype(a)>;
                    if constexpr (Solution::has_remove_v<A>)
                    {
                        return &(a.remove);
                    }
                    else if constexpr (std::is_same_v<A, Solution::Reinstall>)
                    {
                        return &(a.what);
                    }
                    else
                    {
                        return nullptr;
                    }
                },
                action
            );
        }

        template <std::ranges::range Range>
        auto packages_to_remove_impl(Range& actions)
        {
            namespace views = std::ranges::views;
            return actions  //
                   | views::transform([](auto& a) { return detail::to_remove_ptr(a); })
                   | views::filter([](const auto* ptr) { return ptr != nullptr; })
                   | views::transform([](auto* ptr) -> decltype(auto) { return *ptr; });
        }

    }

    inline auto Solution::packages_to_remove() const
    {
        return detail::packages_to_remove_impl(actions);
    }

    inline auto Solution::packages_to_remove()
    {
        return detail::packages_to_remove_impl(actions);
    }

    namespace detail
    {
        template <typename Action>
        auto to_install_ptr(Action& action)
        {
            using PackageInfoPtr = std::
                conditional_t<std::is_const_v<Action>, const specs::PackageInfo*, specs::PackageInfo*>;
            return std::visit(
                [](auto& a) -> PackageInfoPtr
                {
                    using A = std::decay_t<decltype(a)>;
                    if constexpr (Solution::has_install_v<A>)
                    {
                        return &(a.install);
                    }
                    else if constexpr (std::is_same_v<A, Solution::Reinstall>)
                    {
                        return &(a.what);
                    }
                    else
                    {
                        return nullptr;
                    }
                },
                action
            );
        }

        template <std::ranges::range Range>
        auto packages_to_install_impl(Range& actions)
        {
            namespace views = std::ranges::views;
            return actions  //
                   | views::transform([](auto& a) { return detail::to_install_ptr(a); })
                   | views::filter([](const auto* ptr) { return ptr != nullptr; })
                   | views::transform([](auto* ptr) -> decltype(auto) { return *ptr; });
        }

    }

    inline auto Solution::packages_to_install() const
    {
        return detail::packages_to_install_impl(actions);
    }

    inline auto Solution::packages_to_install()
    {
        return detail::packages_to_install_impl(actions);
    }

    namespace detail
    {
        template <typename Action>
        auto to_omit_ptr(Action& action)
        {
            using PackageInfoPtr = std::
                conditional_t<std::is_const_v<Action>, const specs::PackageInfo*, specs::PackageInfo*>;
            return std::visit(
                [](auto& a) -> PackageInfoPtr
                {
                    using A = std::decay_t<decltype(a)>;
                    if constexpr (std::is_same_v<A, Solution::Omit>)
                    {
                        return &(a.what);
                    }
                    else
                    {
                        return nullptr;
                    }
                },
                action
            );
        }

        template <std::ranges::range Range>
        auto packages_to_omit_impl(Range& actions)
        {
            namespace views = std::ranges::views;
            return actions  //
                   | views::transform([](auto& a) { return detail::to_omit_ptr(a); })
                   | views::filter([](const auto* ptr) { return ptr != nullptr; })
                   | views::transform([](auto* ptr) -> decltype(auto) { return *ptr; });
        }

    }

    inline auto Solution::packages_to_omit() const
    {
        return detail::packages_to_omit_impl(actions);
    }

    inline auto Solution::packages_to_omit()
    {
        return detail::packages_to_omit_impl(actions);
    }

    namespace detail
    {
        template <typename Action>
        constexpr auto package_unique_ptrs(Action& action)
        {
            auto out = std::array{
                to_omit_ptr(action),
                to_install_ptr(action),
                to_remove_ptr(action),
            };
            for (std::size_t i = 1; i < out.size(); ++i)
            {
                for (std::size_t j = i + 1; j < out.size(); ++j)
                {
                    if (out[j] == out[i])
                    {
                        out[j] = nullptr;
                    }
                }
            }
            return out;
        }

        template <std::ranges::range Range>
        auto packages_impl(Range& actions)
        {
            namespace views = std::ranges::views;
            return actions                                                             //
                   | views::transform([](auto& a) { return package_unique_ptrs(a); })  //
                   | views::join                                                       //
                   | views::filter([](const auto* ptr) { return ptr != nullptr; })     //
                   | views::transform([](auto* ptr) -> decltype(auto) { return *ptr; });
        }
    }

    inline auto Solution::packages() const
    {
        return detail::packages_impl(actions);
    }

    inline auto Solution::packages()
    {
        return detail::packages_impl(actions);
    }
}
#endif
