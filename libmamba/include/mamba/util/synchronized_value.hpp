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
        };
        //and noexcept(T{}.unlock());

    // see https://en.cppreference.com/w/cpp/named_req/LockableMutex.html
    template<class T>
    concept Lockable = BasicLockable<T>
        and requires(T& x)
        {
            { x.try_lock() } -> std::convertible_to<bool>;
        };

    // see https://en.cppreference.com/w/cpp/named_req/Mutex.html
    template<class T>
    concept Mutex = Lockable<T>
        and std::default_initializable<T>
        and std::destructible<T>
        and (not std::movable<T>)
        and (not std::copyable<T>);


    template<std::semiregular T, Mutex M, bool is_const>
    class scoped_locked_ptr
    {

    };

    template<std::semiregular T, Mutex M = std::mutex>
    class synchronized_value
    {
    public:

        using value_type = T;
        using mutex_type = M;

        synchronized_value() noexcept(std::is_nothrow_default_constructible_v<T>);

        synchronized_value(const synchronized_value& other);
        synchronized_value(synchronized_value&& other);
        synchronized_value& operator=(const synchronized_value& other);
        synchronized_value& operator=(synchronized_value&& other);

        synchronized_value(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);
        synchronized_value(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>);
        synchronized_value& operator=(const T& value);
        synchronized_value& operator=(T&& value);

        void swap(synchronized_value& other);
        void swap(T& value);

        T get() const;
        T& operator*();
        const T& operator*() const;

        using lock_ptr = scoped_locked_ptr<T, M, false>;
        using const_lock_ptr = scoped_locked_ptr<T, M, true>;

        auto synchronize() -> lock_ptr;
        auto synchronize() const -> const_lock_ptr;
        auto operator->() -> lock_ptr;
        auto operator->() const -> const_lock_ptr;

    private:
        T m_value;
        mutable M m_mutex;
    };

}

#endif

