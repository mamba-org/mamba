// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#ifndef LIBMAMBATESTS_UTIL_HPP
#define LIBMAMBATESTS_UTIL_HPP

#include <thread>
#include <functional>
#include <utility>
#include <concepts>

namespace mambatests
{
    // Throws a string immediately, used in tests for code that should not be reachable.
    inline
    void fail_now()
    {
        throw "this code should never be executed";
    }


    /** Blocks the current thread until the provided predicates returns `true`.
        This is useful to make multiple threads wait on the change of value of a
        thread-safe object (for example `std::atomic<bool>`), without having to
        do the complicated dance with `std::condition_variable`.
        Not recommended to use outside testing purpose.
        Calling this will release the thread to the system as much as possible to limit
        thread exhaustion. In consequence it is not possible to decied when exactly the
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