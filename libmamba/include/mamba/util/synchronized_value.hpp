// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_SYNCHRONIZED_VALUE_HPP
#define MAMBA_UTIL_SYNCHRONIZED_VALUE_HPP

#include <concepts>
#include <mutex>

namespace mamba::util
{

    // see https://en.cppreference.com/w/cpp/named_req/BasicLockable.html
    template<class T>
    concept BasicLockable = requires(T& x)
        {
            x.lock();
            x.unlock();
        }
        and noexcept(T{}.unlock());
    static_assert(BasicLockable<std::mutex>);
    static_assert(BasicLockable<std::recursive_mutex>);
    static_assert(BasicLockable<std::shared_mutex>);

    // see https://en.cppreference.com/w/cpp/named_req/LockableMutex.html
    template<class T>
    concept Lockable = BasicLockable<T>
        and requires(T& x)
        {
            { x.try_lock() } -> std::convertible_to<bool>;
        };
    static_assert(Lockable<std::mutex>);
    static_assert(Lockable<std::recursive_mutex>);
    static_assert(Lockable<std::shared_mutex>);

    // see https://en.cppreference.com/w/cpp/named_req/Mutex.html
    template<class T>
    concept Mutex = Lockable<T>
        and std::is_default_contructible_v<T>
        and std::is_destructible_v<T>
        and (not std::is_movable_v<T>)
        and (not std::is_copyable_v<T>);

    static_assert(Mutex<std::mutex>);
    static_assert(Mutex<std::recursive_mutex>);
    static_assert(Mutex<std::shared_mutex>);




}

#endif

