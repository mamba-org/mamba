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
        struct Flags
        {
            /** Keep the dependencies of the install package in the solution. */
            bool keep_dependencies = true;
            /** Keep the original user requested package in the solution. */
            bool keep_user_specs = true;
            /** Force reinstallation of jobs. */
            bool force_reinstall = false;
            /** Allow downgrading packages to satisfy requirements. */
            bool allow_downgrade = true;
            /** Allow uninstalling packages to satisfy requirements. */
            bool allow_uninstall = true;
            /** Prefer packages by repoitory order. */
            bool strict_repo_priority = true;
            /** Order the request to get a deterministic solution. */
            bool order_request = true;
        };

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
            bool clean_dependencies = true;
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

        using Job = std::variant<Install, Remove, Update, UpdateAll, Keep, Freeze, Pin>;
        using job_list = std::vector<Job>;

        Flags flags = {};
        job_list jobs = {};
    };

    template <typename... Item, typename Func>
    void for_each_of(const Request& request, Func&& func)
    {
        for (const auto& unknown_job : request.jobs)
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
                            return util::LoopControl::Continue;
                        }
                    }
                    else
                    {
                        return util::LoopControl::Continue;
                    }
                },
                unknown_job
            );
            if (control == util::LoopControl::Break)
            {
                break;
            }
        }
    }
}
#endif
