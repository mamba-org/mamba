// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#ifndef LIBMAMBATESTS_UTIL_HPP
#define LIBMAMBATESTS_UTIL_HPP

#include <concepts>
#include <functional>
#include <string_view>
#include <thread>
#include <utility>

#include "mamba/core/channel_context.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/unresolved_channel.hpp"

namespace mambatests
{
    /** Resolve a channel string with typical test defaults (conda.anaconda.org, linux-64/noarch).
     */
    [[nodiscard]] inline auto make_simple_channel(std::string_view chan) -> mamba::specs::Channel
    {
        const auto resolve_params = mamba::ChannelContext::ChannelResolveParams{
            { "linux-64", "noarch" },
            mamba::specs::CondaURL::parse("https://conda.anaconda.org").value()
        };

        return mamba::specs::Channel::resolve(
                   mamba::specs::UnresolvedChannel::parse(chan).value(),
                   resolve_params
        )
            .value()
            .front();
    }

    /** Throws a string immediately, used in tests for code that should not be reachable. */
    inline void fail_now()
    {
        throw "this code should never be executed";
    }

    /** Blocks the current thread until the provided predicates returns `true`.
        This is useful to make multiple threads wait on the change of value of a
        thread-safe object (for example `std::atomic<bool>`), without having to
        do the complicated dance with `std::condition_variable`.
        Not recommended to use outside testing purpose.
        Calling this will release the thread to the system as much as possible to limit
        thread exhaustion. In consequence it is not possible to decide when exactly the
        predicate will be evaluated, it depends on when the system resumes the thread.
    */
    template <std::predicate<> Predicate>
    void wait_condition(Predicate&& predicate)
    {
        while (!std::invoke(std::forward<Predicate>(predicate)))
        {
            std::this_thread::yield();
        }
    }

}
#endif
