// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_SYNCHRONIZED_VALUE_HPP
#define MAMBA_UTIL_SYNCHRONIZED_VALUE_HPP

#include <concepts>
#include <mutex>
#include <tuple>

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


    template< std::default_initializable T, Mutex M, bool readonly >
    class scoped_locked_ptr
    {
        T* m_value;
        std::scoped_lock<M> m_lock;

    public:
        scoped_locked_ptr(T& value, M& mutex)
        : m_value(&value), m_lock(mutex)
        {}

        scoped_locked_ptr(scoped_locked_ptr&& other) noexcept
            : m_value(std::move(other.m_value)), m_lock(std::move(other.m_lock))
        {
            other.m_value = nullptr;
        }

        auto operator*() -> T& requires(not readonly)  { return *m_value; }
        auto operator*() const -> const T& { return *m_value; }
        auto operator->() -> T*  requires(not readonly) { return m_value; }
        auto operator->() const -> const T* { return m_value; }


    };

    template< std::default_initializable T, Mutex M, bool readonly >
    class locking_ref
    {
        T& m_ref;
        std::scoped_lock<M> m_lock;
    private:

        locking_ref(T& value, M& mutex)
            : m_ref(value), m_lock(mutex)
        {}

        operator T& () requires(not readonly) { return m_ref; }
        operator const T& () const requires(readonly) { return m_ref; }

        template<typename V>
        locking_ref& operator=( V&& new_value )
            requires(not readonly) and std::assignable_from<T&, V>
        {
            m_ref = std::forward<V>(new_value);
            return *this;
        }

    };

    template< std::default_initializable T, Mutex M = std::mutex >
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

        template< typename V >
            requires std::assignable_from<T&, V>
        synchronized_value(V&& value);

        template< typename V >
            requires std::assignable_from<T&, V>
        synchronized_value& operator=(V&& value);

        auto swap(synchronized_value& other) -> void;
        auto swap(T& value) -> void;

        auto value() const -> T;
        explicit operator T() const { return value(); }

        auto unsafe_get() const -> const T& { return m_value; }
        auto unsafe_get() -> T& { return m_value; }


        using locking_ref = mamba::util::locking_ref<T, M, true>;
        using locking_const_ref = mamba::util::locking_ref<T, M, false>;

        auto operator*() -> locking_ref;
        auto operator*() const -> locking_const_ref;
        auto operator->() -> locking_ref;
        auto operator->() const -> locking_const_ref;

        using lock_ptr = mamba::util::scoped_locked_ptr<T, M, false>;
        using const_lock_ptr = mamba::util::scoped_locked_ptr<T, M, true>;

        auto synchronize() -> lock_ptr;
        auto synchronize() const -> const_lock_ptr;

        template< std::invocable<T> Func, typename... Args >
        auto apply(Func&& func, Args&&... args);

        template< std::invocable<T> Func, typename... Args >
        auto apply(Func&& func, Args&&... args) const;

        template< std::invocable<T> Func, typename... Args >
        auto operator()(Func&& func, Args&&... args) { return apply(std::forward<Func>(func), std::forward<Args>(args)...); }

        template< std::invocable<T> Func, typename... Args >
        auto operator()(Func&& func, Args&&... args) const { return apply(std::forward<Func>(func), std::forward<Args>(args)...); }

    private:
        T m_value;
        mutable M m_mutex;
    };

    template< std::default_initializable... Ts, Mutex... Ms, bool... is_const >
    auto synchronize(synchronized_value<Ts, Ms>&&... sync_values) -> std::tuple<scoped_locked_ptr<Ts, Ms, is_const>...>;

    ///////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////

    template< std::default_initializable T, Mutex M >
    synchronized_value<T, M>::synchronized_value()
        noexcept(std::is_nothrow_default_constructible_v<T>)
        = default;

    template< std::default_initializable T, Mutex M >
    synchronized_value<T, M>::synchronized_value(const synchronized_value& other)
    {
        std::scoped_lock _ {other.m_mutex};
        m_value = other.m_value;
    }


    template< std::default_initializable T, Mutex M >
    synchronized_value<T, M>&
    synchronized_value<T, M>::operator=(const synchronized_value& other)
    {
        std::scoped_lock _ { m_mutex, other.m_mutex };
        m_value = other.m_value;
        return *this;
    }

    template< std::default_initializable T, Mutex M >
    synchronized_value<T, M>::synchronized_value(synchronized_value&& other) = default;

    template< std::default_initializable T, Mutex M >
    synchronized_value<T, M>&
    synchronized_value<T, M>::operator=(synchronized_value&& other) = default;

    template< std::default_initializable T, Mutex M >
    template< typename V >
        requires std::assignable_from<T&, V>
    synchronized_value<T, M>::synchronized_value(V&& value)
        : m_value(std::forward<V>(value))
    {}

    template< std::default_initializable T, Mutex M >
    template< typename V >
        requires std::assignable_from<T&, V>
    synchronized_value<T, M>&
    synchronized_value<T, M>::operator=(V&& value)
    {
        std::scoped_lock _ { m_mutex };
        m_value = std::forward<V>(value);
        return *this;
    }


    template< std::default_initializable T, Mutex M >
    auto synchronized_value<T, M>::value() const -> T
    {
        std::scoped_lock _ { m_mutex };
        return m_value;
    }

    template< std::default_initializable T, Mutex M >
    auto synchronized_value<T, M>::operator*() -> locking_ref
    {
        return locking_ref{ m_value, m_mutex };
    }

    template< std::default_initializable T, Mutex M >
    auto synchronized_value<T, M>::operator*() const -> locking_const_ref
    {
        return locking_const_ref{ m_value, m_mutex };
    }

    template< std::default_initializable T, Mutex M >
    auto synchronized_value<T, M>::operator->() -> locking_ref
    {
        return locking_ref{ m_value, m_mutex };
    }

    template< std::default_initializable T, Mutex M >
    auto synchronized_value<T, M>::operator->() const -> locking_const_ref
    {
        return locking_const_ref{ m_value, m_mutex };
    }

    template< std::default_initializable T, Mutex M >
    auto synchronized_value<T, M>::synchronize() -> lock_ptr
    {
        return lock_ptr{ m_value, m_mutex };
    }

    template< std::default_initializable T, Mutex M >
    auto synchronized_value<T, M>::synchronize() const -> const_lock_ptr
    {
        return const_lock_ptr{ m_value, m_mutex };
    }

    template< std::default_initializable T, Mutex M >
    template< std::invocable<T> Func, typename... Args >
    auto synchronized_value<T, M>::apply(Func&& func, Args&&... args)
    {
        std::scoped_lock _ { m_mutex };
        return std::invoke(func, m_value, std::forward<Args>(args)...);
    }

    template< std::default_initializable T, Mutex M >
    template< std::invocable<T> Func, typename... Args  >
    auto synchronized_value<T, M>::apply(Func&& func, Args&&... args) const
    {
        std::scoped_lock _ { m_mutex };
        return std::invoke(func, m_value, std::forward<Args>(args)...);
    }


    template< std::default_initializable T, Mutex M >
    auto synchronized_value<T, M>::swap(synchronized_value& other) -> void
    {
        std::scoped_lock _ { m_mutex, other.m_mutex };
        using std::swap;
        swap(m_value, other.m_value);
    }


    template< std::default_initializable T, Mutex M >
    auto synchronized_value<T, M>::swap(T& value) -> void
    {
        std::scoped_lock _ { m_mutex };
        using std::swap;
        swap(m_value, value);
    }

}

#endif

