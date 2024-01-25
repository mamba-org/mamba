// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLVER_REQUEST_HPP
#define MAMBA_SOLVER_REQUEST_HPP

#include <type_traits>
#include <variant>

#include "mamba/specs/match_spec.hpp"
#include "mamba/util/loop_control.hpp"
#include "mamba/util/type_traits.hpp"

namespace mamba::solver
{
    struct Request
    {
        /** Instruct to install a package matching the given spec. */
        struct Install
        {
            specs::MatchSpec spec;
        };

        /** Instruct to remove packages matching the given spec. */
        struct Remove
        {
            specs::MatchSpec spec;
            bool clean_dependencies = true;
        };

        /** Instruct to update packages matching the given spec. */
        struct Update
        {
            specs::MatchSpec spec;
        };

        /** Instruct to update all installed packages. */
        struct UpdateAll
        {
            bool clean_dependencies = true;
        };

        /** Instruct to keep packages matching the spec during dependency clean. */
        struct Keep
        {
            specs::MatchSpec spec;
        };

        /** Instruct to freeze the version and status of the matching installed package. */
        struct Freeze
        {
            specs::MatchSpec spec;
        };

        /** Add a constraints on packages possible to install. */
        struct Pin
        {
            specs::MatchSpec spec;
        };

        using Item = std::variant<Install, Remove, Update, UpdateAll, Keep, Freeze, Pin>;
        using item_list = std::vector<Item>;

        item_list items = {};
    };

    template <typename... Item, typename Func>
    void for_each_of(const Request& request, Func&& func)
    {
        for (const auto& unknown_itm : request.items)
        {
            const auto control = std::visit(
                [&](const auto& itm) -> util::LoopControl
                {
                    using Itm = std::decay_t<decltype(itm)>;
                    if constexpr (util::is_any_of_v<Itm, Item...>)
                    {
                        if constexpr (std::is_same_v<decltype(func(itm)), util::LoopControl>)
                        {
                            return func(itm);
                        }
                        else
                        {
                            func(itm);
                        }
                    }
                    return util::LoopControl::Continue;
                },
                unknown_itm
            );
            if (control == util::LoopControl::Break)
            {
                break;
            }
        }
    }
}
#endif
