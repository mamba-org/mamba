// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_SYNCHRONIZED_VALUE_HPP
#define MAMBA_UTIL_SYNCHRONIZED_VALUE_HPP

#include <concepts>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <tuple>
#include <utility>

namespace mamba::util
{
    /////////////////////////////
    // TODO: move that in a more general location
    // original: https://github.com/man-group/sparrow/blob/main/include/sparrow/utils/mp_utils.hpp


    template <class L, template <class...> class U>
    struct is_type_instance_of : std::false_type
    {
    };

    template <template <class...> class L, template <class...> class U, class... T>
        requires std::same_as<L<T...>, U<T...>>
    struct is_type_instance_of<L<T...>, U> : std::true_type
    {
    };

    /// `true` if `T` is a concrete type template instantiation of `U` which is a type template.
    /// Example: is_type_instance_of_v< std::vector<int>, std::vector > == true
    template <class T, template <class...> class U>
    constexpr bool is_type_instance_of_v = is_type_instance_of<T, U>::value;

    /// `true` if the instances of two provided types can be compared with operator==.
    /// Notice that this concept is less restrictive than `std::equality_comparable_with`,
    /// which requires the existence of a common reference type for T and U. This additional
    /// restriction makes it impossible to use it in the context here (originally of sparrow), where
    /// we want to compare objects that are logically similar while being "physically" different.
    // Source:
    // https://github.com/man-group/sparrow/blob/66f70418cf1b00cc294c99bbbe04b5b4d2f83c98/include/sparrow/utils/mp_utils.hpp#L604-L619

    template <class T, class U>
    concept weakly_equality_comparable_with = requires(
        const std::remove_reference_t<T>& t,
        const std::remove_reference_t<U>& u
    ) {
        { t == u } -> std::convertible_to<bool>;
        { t != u } -> std::convertible_to<bool>;
        { u == t } -> std::convertible_to<bool>;
        { u != t } -> std::convertible_to<bool>;
    };

    template <class T, class U>
    concept weakly_assignable_from = requires(T t, U&& u) { t = std::forward<U>(u); };


    /////////////////////////////


    /// see https://en.cppreference.com/w/cpp/named_req/BasicLockable.html
    template <class T>
    concept BasicLockable = requires(T& x) {
        x.lock();
        x.unlock();
    };
    // and noexcept(T{}.unlock());

    /// see https://en.cppreference.com/w/cpp/named_req/LockableMutex.html
    template <class T>
    concept Lockable = BasicLockable<T> and requires(T& x) {
        { x.try_lock() } -> std::convertible_to<bool>;
    };

    /// see https://en.cppreference.com/w/cpp/named_req/Mutex.html
    template <class T>
    concept Mutex = Lockable<T> and std::default_initializable<T> and std::destructible<T>
                    and (not std::movable<T>) and (not std::copyable<T>);

    /// see https://en.cppreference.com/w/cpp/named_req/SharedMutex.html
    template <class T>
    concept SharedMutex = Mutex<T> and requires(T& x) {
        x.lock_shared();
        { x.try_lock_shared() } -> std::convertible_to<bool>;
        x.unlock_shared();
    };

    /** Locks a mutex object using the most constrained sharing lock available for that mutex type.
        @returns A scoped locking object. The exact type depends on the mutex type.
    */
    template <Mutex M>
    [[nodiscard]]
    auto lock_as_readonly(M& mutex)
    {
        return std::unique_lock{ mutex };
    }

    template <SharedMutex M>
    [[nodiscard]]
    auto lock_as_readonly(M& mutex)
    {
        return std::shared_lock{ mutex };
    }

    /** Locks multiple mutex objects using the most constrained sharing lock available for that
        mutex type.
        @returns A tuple of scoped locking objects, one for each mutex. The exact types depends on
        the mutex types.
    */
    template <Mutex... M>
        requires(sizeof...(M) > 1) and (SharedMutex<M> or ...)
    [[nodiscard]] auto lock_as_readonly(M&... mutex)
    {
        return std::make_tuple(lock_as_readonly(mutex)...);
    }

    /** Locks multiple non-shared mutex objects using the most constrained sharing lock available
        for that mutex type.
        @returns A scoped locking object.
    */
    template <Mutex... M>
        requires(sizeof...(M) > 1) and ((not SharedMutex<M>) and ...)
    [[nodiscard]] auto lock_as_readonly(M&... mutex)
    {
        return std::scoped_lock{ mutex... };
    }

    /** Locks a mutex object using an exclusive lock.
        @returns A scoped locking object.
    */
    template <Mutex M>
    [[nodiscard]]
    auto lock_as_exclusive(M& mutex)
    {
        return std::unique_lock{ mutex };
    }

    /** Locks multiple mutex objects using an exclusive lock.
        @returns A scoped locking object.
    */
    template <Mutex... M>
        requires(sizeof...(M) > 1)
    [[nodiscard]]
    auto lock_as_exclusive(M&... mutex)
    {
        return std::scoped_lock{ mutex... };
    }

    namespace details
    {
        template <typename T>
        T& ref_of();  // used only in non-executed contexts
    }

    /** Scoped locking type that would result from locking the provided mutex in the most
        constrained way.
    */
    template <Mutex M, bool readonly>
    using lock_type = std::conditional_t<
        readonly,
        decltype(lock_as_readonly(details::ref_of<M>())),
        decltype(lock_as_exclusive(details::ref_of<M>()))>;

    /** Locks a mutex for the lifetime of this type's instance and provide access to an associated
        value.

        If `readonly == true`, only non-mutable access to the associated value will be provided.
        The access to the value is pointer-like, but this type does not own or copy that value,
        it is accessed directly.
    */
    template <std::default_initializable T, Mutex M, bool readonly>
    class [[nodiscard]] scoped_locked_ptr
    {
        std::conditional_t<readonly, const T*, T*> m_value;
        lock_type<M, readonly> m_lock;

    public:

        static constexpr bool is_readonly = readonly;

        /** Locks the provided mutex immediately.
            The provided value will then be accessible as mutable through the member functions.
        */
        scoped_locked_ptr(T& value, M& mutex)
            requires(not readonly)
            : m_value(&value)
            , m_lock(mutex)
        {
        }

        /** Locks the provided mutex immediately.
            The provided value will then be accessible as non-mutable through the member functions.
        */
        scoped_locked_ptr(const T& value, M& mutex)
            requires(readonly)
            : m_value(&value)
            , m_lock(mutex)
        {
        }

        scoped_locked_ptr(scoped_locked_ptr&& other) noexcept
            // Both objects are locking at this point, so it is safe to modify both values.
            : m_value(std::move(other.m_value))
            , m_lock(std::move(other.m_lock))
        {
            other.m_value = nullptr;
        }

        scoped_locked_ptr& operator=(scoped_locked_ptr&& other) noexcept
        {
            // Both objects are locking at this point, so it is safe to modify both values.
            m_value = std::move(other.m_value);
            other.m_value = nullptr;
            return *this;
        }

        [[nodiscard]] auto operator*() -> T& requires(not readonly) { return *m_value; }
        [[nodiscard]] auto operator*() const -> const T&
        {
            return *m_value;
        }

        [[nodiscard]] auto
        operator->() -> T* requires(not readonly) { return m_value; }
        [[nodiscard]] auto operator->() const -> const T*
        {
            return m_value;
        }
    };

    /** Thread-safe value storage.

        Holds an object which access is always implying a lock to an associated mutex. The only
        access to the object without a lock are "unsafe" functions, which are named as such. Also
        provides ways to lock the access to the object for a whole scope.

        Mainly used when a value needs to be protected by a mutex and we want to make sure the code
        always does the right locking mechanism.

        If the mutex type satisfies `SharedMutex`, the locks will be shared if using `const`
        functions, enabling cheaper read-only access to the object in that context.

        Some operations will lock for the time of the call, others (like `operator->`) will
        return a `scoped_locked_ptr` so that the lock will hold for a whole expression or
        a bigger scope. `synchronize()` explicitly only builds such scoped-lock and provides it
        for scoped usage of the object.

        This type is as move-enabled and copy-enabled as it's stored object's type.

        Note: this is inspired by boost::thread::synchronized_value and the C++ Concurrent TS 2
        paper, refer to these to compare the features and correctness.
    */
    template <std::default_initializable T, Mutex M = std::mutex>
    class synchronized_value
    {
    public:

        using value_type = T;
        using mutex_type = M;
        using this_type = synchronized_value<T, M>;

        synchronized_value() noexcept(std::is_nothrow_default_constructible_v<T>);

        /// Constructs with a provided value as initializer for the stored object.
        template <typename V>
            requires(not std::same_as<T, std::decay_t<V>>)
                    and (not std::same_as<this_type, std::decay_t<V>>)
                    and (not is_type_instance_of_v<std::decay_t<V>, synchronized_value>)
                    and weakly_assignable_from<T&, V>
        synchronized_value(V&& value) noexcept
            : m_value(std::forward<V>(value))
        {
            // NOTE: when moving the definition outside the class,
            // VS2022 will not match the declaration with the definition
            // which is probably a bug. To workaround that we keep
            // the definition here.
        }

        /// Constructs with a provided value as initializer for the stored object.
        // NOTE: this is redundant with the generic impl, but required to workaround
        // apple-clang failing to properly constrain the generic impl.
        synchronized_value(T value) noexcept;

        /// Constructs with a provided initializer list used to initialize the stored object.
        template <typename V>
            requires std::constructible_from<T, std::initializer_list<V>>
        synchronized_value(std::initializer_list<V> values);

        /** Locks the provided `synchronized_value`'s mutex and copies it's stored object value
            to this instance's stored object.
            If `SharedMutex<M> == true`, the lock is a shared-lock for the provided
           `synchronized_value`'s mutex.
            The lock is released before the end of the call.
        */
        synchronized_value(const synchronized_value& other);

        /** Locks the provided `synchronized_value`'s mutex and moves it's stored object value
            into this instance's stored object.
            The lock is exclusive.
            The lock is released before the end of the call.
        */
        synchronized_value(synchronized_value&& other) noexcept;

        /** Locks the provided `synchronized_value`'s mutex and copies it's stored object value
            to this instance's stored object.
            If `SharedMutex<M> == true`, the lock is a shared-lock for the provided
           `synchronized_value`'s mutex.
            The lock is released before the end of the call.
        */
        template <std::default_initializable U, Mutex OtherMutex>
            requires(not std::same_as<synchronized_value<T, M>, synchronized_value<U, OtherMutex>>)
                    and std::constructible_from<T, U>
        synchronized_value(const synchronized_value<U, OtherMutex>& other);

        /** Locks the provided `synchronized_value`'s mutex and moves it's stored object value
            into this instance's stored object.
            The lock is exclusive.
            The lock is released before the end of the call.
        */
        template <std::default_initializable U, Mutex OtherMutex>
            requires(not std::same_as<synchronized_value<T, M>, synchronized_value<U, OtherMutex>>)
                    and std::constructible_from<T, U&&>
        synchronized_value(synchronized_value<U, OtherMutex>&& other) noexcept;

        /** Locks both mutexes and copies the value of the provided `synchronized_value`'s
            stored object to this instance's stored object.
            If `SharedMutex<M> == true`, the lock is a shared-lock for the provided
           `synchronized_value`'s mutex.
            The lock is released before the end of the call.
        */
        auto operator=(const synchronized_value& other) -> synchronized_value&;

        /** Locks both mutexes and moves the value of the provided `synchronized_value`'s
            stored object to this instance's stored object.
            For both, the lock is exclusive.
            The lock is released before the end of the call.
            Only available if a `U` instance can be moved into `T` instance.
        */
        auto operator=(synchronized_value&& other) noexcept -> synchronized_value&;

        /** Locks both mutexes and copies the value of the provided `synchronized_value`'s
            stored object to this instance's stored object.
            If `SharedMutex<M> == true`, the lock is a shared-lock for the provided
           `synchronized_value`'s mutex.
            The lock is released before the end of the call.
        */
        template <std::default_initializable U, Mutex OtherMutex>
            requires(not std::same_as<synchronized_value<T, M>, synchronized_value<U, OtherMutex>>)
                    and weakly_assignable_from<T&, U>
        auto operator=(const synchronized_value<U, OtherMutex>& other) -> synchronized_value&;


        /** Locks both mutexes and moves the value of the provided `synchronized_value`'s
            stored object to this instance's stored object.
            For both, the lock is exclusive.
            The lock is released before the end of the call.
            Only available if a `U` instance can be moved into `T` instance.
        */
        template <std::default_initializable U, Mutex OtherMutex>
            requires(not std::same_as<synchronized_value<T, M>, synchronized_value<U, OtherMutex>>)
                    and weakly_assignable_from<T&, U&&>
        auto operator=(synchronized_value<U, OtherMutex>&& other) noexcept -> synchronized_value&;

        /** Locks and assign the provided value to the stored object.
            The lock is released before the end of the call.
        */
        template <typename V>
            requires(not std::same_as<T, std::decay_t<V>>)
                    and (not std::same_as<this_type, std::decay_t<V>>)
                    and (not is_type_instance_of_v<std::decay_t<V>, synchronized_value>)
                    and weakly_assignable_from<T&, V>
        auto operator=(V&& value) noexcept -> synchronized_value&
        {
            // NOTE: when moving the definition outside the class,
            // VS2022 will not match the declaration with the definition
            // which is probably a bug. To workaround that we keep
            // the definition here.
            auto _ = lock_as_exclusive(m_mutex);
            m_value = std::forward<V>(value);
            return *this;
        }

        /** Locks and assign the provided value to the stored object.
            The lock is released before the end of the call.
        */
        // NOTE: this is redundant with the generic impl, but required to workaround
        // apple-clang failing to properly constrain the generic impl.
        auto operator=(const T& value) noexcept -> synchronized_value&;

        /** Locks and return the value of the current object.
            The lock is released before the end of the call.
            If `SharedMutex<M> == true`, the lock is a shared-lock.
        */
        [[nodiscard]]
        auto value() const -> T;

        /** Locks and return the value of the current object.
            If `SharedMutex<M> == true`, the lock is a shared-lock.
            The lock is released before the end of the call.
        */
        [[nodiscard]]
        explicit operator T() const
        {
            return value();
        }

        /** Not-thread-safe access to the stored object.
            Only used this for testing purposes.
        */
        [[nodiscard]]
        auto unsafe_get() const -> const T&
        {
            return m_value;
        }

        /** Not-thread-safe access to the stored object.
            Only used this for testing purposes.
        */
        [[nodiscard]]
        auto unsafe_get() -> T&
        {
            return m_value;
        }

        using locked_ptr = scoped_locked_ptr<T, M, false>;
        using const_locked_ptr = scoped_locked_ptr<T, M, true>;

        /** Locks the mutex and returns a `scoped_locked_ptr` which will provide
            mutable access to the stored object, while holding the lock for it's whole
            lifetime, which usually for this call is for the time of the expression.

            The lock is released only once the returned object is destroyed.

            Example:
                synchronized_value<std::vector<int>> values;
                values->resize(10); // locks, calls `std::vector::resize`, then unlocks.
        */
        [[nodiscard]]
        auto operator->() -> locked_ptr;

        /** Locks the mutex and returns a `scoped_locked_ptr` which will provide
            non-mutable access to the stored object, while holding the lock for it's whole
            lifetime, which usually for this call is for the time of the expression.
            If `SharedMutex<M> == true`, the lock is a shared-lock.
            The lock is released only once the returned object is destroyed.

            Example:
                synchronized_value<std::vector<int>> values;
                auto x = values->size(); // locks, calls `std::vector::size`, then unlocks.
        */
        [[nodiscard]]
        auto operator->() const -> const_locked_ptr;

        /** Locks the mutex and returns a `scoped_locked_ptr` which will provide
            mutable access to the stored object, while holding the lock for it's whole
            lifetime.
            The lock is released only once the returned object is destroyed.

            This is mainly used to get exclusive mutable access to the stored object for a whole
            scope. Example: synchronized_value<std::vector<int>> values;
                {
                    auto sync_values = values.synchronize(); // locks
                    const auto x = sync_values->size();
                    sync_values->resize(x);
                    // ... maybe more mutable operations ...
                } // unlocks

        */
        [[nodiscard]]
        auto synchronize() -> locked_ptr;

        /** Locks the mutex and returns a `scoped_locked_ptr` which will provide
            non-mutable access to the stored object, while holding the lock for it's whole
            lifetime.
            If `SharedMutex<M> == true`, the lock is a shared-lock.
            The lock is released only once the returned object is destroyed.

            This is mainly used to make sure the stored object doesnt change for a whole scope.
            Example:
                synchronized_value<std::vector<int>> values;
                {
                    auto sync_values = values.synchronize(); // locks
                    const auto x = sync_values->size();
                    // ... more non-mutable operations ...
                } // unlocks

        */
        [[nodiscard]]
        auto synchronize() const -> const_locked_ptr;

        /** Locks the mutex and calls the provided invocable, passing the mutable stored object
            and the other provided values as arguments.
            The lock is released after the provided invocable returns but before this function
            returns.

            This is mainly used to safely execute an already existing function taking the stored
            object as parameter. Example:

                synchronized_value<std::vector<int>> values{ random_values };
                values.apply(std::ranges::sort); // locks, sort, unlocks
                values.apply(std::ranges::sort, std::ranges::greater{}); // locks, reverse sort,
                                                                         // unlocks
                values.apply([](std::vector<int>& vs, auto& input){ // locks
                    for(int& value : vs)
                        input >> value;
                }], file_stream); // unlocks

         */
        template <typename Func, typename... Args>
            requires std::invocable<Func, T&, Args...>
        auto apply(Func&& func, Args&&... args);

        /** Locks the mutex and calls the provided invocable, passing the non-mutable stored object
            and the other provided values as arguments.
            The lock is released after the provided invocable returns but before this function
            returns.
            If `SharedMutex<M> == true`, the lock is a shared-lock.

            This is mainly used to safely execute an already existing function taking the stored
            object as parameter. Example:

                synchronized_value<std::vector<int>> values{ random_values };
                values.apply([](const std::vector<int>& vs, auto& out){ // locks
                    for(int value : vs)
                        out << value;
                }], file_stream); // unlocks

        */
        template <typename Func, typename... Args>
            requires std::invocable<Func, T&, Args...>
        auto apply(Func&& func, Args&&... args) const;

        /// @see `apply()`
        template <typename Func, typename... Args>
            requires std::invocable<Func, T&, Args...>
        auto operator()(Func&& func, Args&&... args)
        {
            return apply(std::forward<Func>(func), std::forward<Args>(args)...);
        }

        /// @see `apply()`
        template <typename Func, typename... Args>
            requires std::invocable<Func, T&, Args...>
        auto operator()(Func&& func, Args&&... args) const
        {
            return apply(std::forward<Func>(func), std::forward<Args>(args)...);
        }

        // TODO : ADD MORE COMPARISON OPERATORS
        /** Locks (shared if possible) and compare equality of the stored object's value with the
            provided value.
        */
        auto operator==(const weakly_equality_comparable_with<T> auto& other_value) const -> bool;

        /** Locks both (shared if possible) and compare equality of the stored object's value with
           the provided value.
        */
        template <weakly_equality_comparable_with<T> U, Mutex OtherMutex>
        auto operator==(const synchronized_value<U, OtherMutex>& other_value) const -> bool;

        auto swap(synchronized_value& other) -> void;
        auto swap(T& value) -> void;

    private:

        T m_value;
        mutable M m_mutex;

        template <std::default_initializable, Mutex>
        friend class synchronized_value;
    };

    /** Locks all the provided `synchronized_value` objects using `.synchronize` and
        returns the resulting set of `scoped_locked_ptr`.
        Used to keep a lock on multiple values at a time under for the lifetime of one same scope.

        @see `synchronized_value::synchronize()`

        @tparam SynchronizedValues Must be `synchronized_value` type instances.

        @param sync_values Various `synchronized_value` objects with potentially different mutex
                           types and value types. Any of these objects that is provided through
                           a `const &` will result in a shared-lock for that object.

        @returns A tuple of `scoped_locked_ptr`, one for each `sync_values` object, in the same
                 order. If an object in `sync_values` was passed using `const &`, then for the
                 associated `scoped_locked_ptr` `scoped_locked_ptr::is_readonly == true`.
    */
    template <typename... SynchronizedValues>
        requires(is_type_instance_of_v<std::remove_cvref_t<SynchronizedValues>, synchronized_value> and ...)
    auto synchronize(SynchronizedValues&&... sync_values);

    ///////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////

    template <std::default_initializable T, Mutex M>
    synchronized_value<T, M>::synchronized_value() noexcept(std::is_nothrow_default_constructible_v<T>) = default;

    template <std::default_initializable T, Mutex M>
    synchronized_value<T, M>::synchronized_value(T value) noexcept
        : m_value(std::move(value))
    {
    }

    template <std::default_initializable T, Mutex M>
    synchronized_value<T, M>::synchronized_value(const synchronized_value& other)
    {
        auto _ = lock_as_readonly(other.m_mutex);
        m_value = other.m_value;
    }

    template <std::default_initializable T, Mutex M>
    synchronized_value<T, M>::synchronized_value(synchronized_value&& other) noexcept
    {
        auto _ = lock_as_exclusive(other.m_mutex);
        m_value = std::move(other.m_value);
    }

    template <std::default_initializable T, Mutex M>
    template <std::default_initializable U, Mutex OtherMutex>
        requires(not std::same_as<synchronized_value<T, M>, synchronized_value<U, OtherMutex>>)
                and std::constructible_from<T, U>
    synchronized_value<T, M>::synchronized_value(const synchronized_value<U, OtherMutex>& other)
    {
        auto _ = lock_as_readonly(other.m_mutex);
        m_value = other.m_value;
    }

    template <std::default_initializable T, Mutex M>
    template <std::default_initializable U, Mutex OtherMutex>
        requires(not std::same_as<synchronized_value<T, M>, synchronized_value<U, OtherMutex>>)
                and std::constructible_from<T, U&&>
    synchronized_value<T, M>::synchronized_value(synchronized_value<U, OtherMutex>&& other) noexcept
    {
        auto _ = lock_as_exclusive(other.m_mutex);
        m_value = std::move(other.m_value);
    }

    template <std::default_initializable T, Mutex M>
    auto synchronized_value<T, M>::operator=(const synchronized_value& other) -> synchronized_value&
    {
        auto this_lock [[maybe_unused]] = lock_as_exclusive(m_mutex);
        auto other_lock [[maybe_unused]] = lock_as_readonly(other.m_mutex);
        m_value = other.m_value;
        return *this;
    }

    template <std::default_initializable T, Mutex M>
    auto synchronized_value<T, M>::operator=(synchronized_value&& other) noexcept
        -> synchronized_value&
    {
        auto _ = lock_as_exclusive(other.m_mutex, m_mutex);
        m_value = std::move(other.m_value);
        return *this;
    }

    template <std::default_initializable T, Mutex M>
    template <std::default_initializable U, Mutex OtherMutex>
        requires(not std::same_as<synchronized_value<T, M>, synchronized_value<U, OtherMutex>>)
                and weakly_assignable_from<T&, U>
    auto synchronized_value<T, M>::operator=(const synchronized_value<U, OtherMutex>& other)
        -> synchronized_value<T, M>&
    {
        auto this_lock [[maybe_unused]] = lock_as_exclusive(m_mutex);
        auto other_lock [[maybe_unused]] = lock_as_readonly(other.m_mutex);
        m_value = other.m_value;
        return *this;
    }

    template <std::default_initializable T, Mutex M>
    template <std::default_initializable U, Mutex OtherMutex>
        requires(not std::same_as<synchronized_value<T, M>, synchronized_value<U, OtherMutex>>)
                and weakly_assignable_from<T&, U&&>
    auto synchronized_value<T, M>::operator=(synchronized_value<U, OtherMutex>&& other) noexcept
        -> synchronized_value<T, M>&
    {
        auto _ = lock_as_exclusive(other.m_mutex, m_mutex);
        m_value = std::move(other.m_value);
        return *this;
    }

    template <std::default_initializable T, Mutex M>
    auto synchronized_value<T, M>::operator=(const T& value) noexcept -> synchronized_value&
    {
        auto _ = lock_as_exclusive(m_mutex);
        m_value = value;
        return *this;
    }

    template <std::default_initializable T, Mutex M>
    template <typename V>
        requires std::constructible_from<T, std::initializer_list<V>>
    synchronized_value<T, M>::synchronized_value(std::initializer_list<V> values)
        : m_value(std::move(values))
    {
    }

    template <std::default_initializable T, Mutex M>
    auto synchronized_value<T, M>::value() const -> T
    {
        auto _ = lock_as_readonly(m_mutex);
        return m_value;
    }

    template <std::default_initializable T, Mutex M>
    auto synchronized_value<T, M>::operator->() -> locked_ptr
    {
        return locked_ptr{ m_value, m_mutex };
    }

    template <std::default_initializable T, Mutex M>
    auto synchronized_value<T, M>::operator->() const -> const_locked_ptr
    {
        return const_locked_ptr{ m_value, m_mutex };
    }

    template <std::default_initializable T, Mutex M>
    auto synchronized_value<T, M>::synchronize() -> locked_ptr
    {
        return locked_ptr{ m_value, m_mutex };
    }

    template <std::default_initializable T, Mutex M>
    auto synchronized_value<T, M>::synchronize() const -> const_locked_ptr
    {
        return const_locked_ptr{ m_value, m_mutex };
    }

    template <std::default_initializable T, Mutex M>
    template <typename Func, typename... Args>
        requires std::invocable<Func, T&, Args...>
    auto synchronized_value<T, M>::apply(Func&& func, Args&&... args)
    {
        auto _ = lock_as_exclusive(m_mutex);
        return std::invoke(std::forward<Func>(func), m_value, std::forward<Args>(args)...);
    }

    template <std::default_initializable T, Mutex M>
    template <typename Func, typename... Args>
        requires std::invocable<Func, T&, Args...>
    auto synchronized_value<T, M>::apply(Func&& func, Args&&... args) const
    {
        auto _ = lock_as_readonly(m_mutex);
        return std::invoke(std::forward<Func>(func), std::as_const(m_value), std::forward<Args>(args)...);
    }

    template <std::default_initializable T, Mutex M>
    auto
    synchronized_value<T, M>::operator==(const weakly_equality_comparable_with<T> auto& other_value
    ) const -> bool
    {
        auto _ = lock_as_readonly(m_mutex);
        return m_value == other_value;
    }

    template <std::default_initializable T, Mutex M>
    template <weakly_equality_comparable_with<T> U, Mutex OtherMutex>
    auto
    synchronized_value<T, M>::operator==(const synchronized_value<U, OtherMutex>& other_value) const
        -> bool
    {
        auto _ = lock_as_readonly(m_mutex, other_value.m_mutex);
        return m_value == other_value.m_value;
    }

    template <std::default_initializable T, Mutex M>
    auto synchronized_value<T, M>::swap(synchronized_value& other) -> void
    {
        auto _ = lock_as_exclusive(m_mutex, other.m_mutex);
        using std::swap;
        swap(m_value, other.m_value);
    }

    template <std::default_initializable T, Mutex M>
    auto synchronized_value<T, M>::swap(T& value) -> void
    {
        auto _ = lock_as_exclusive(m_mutex);
        using std::swap;
        swap(m_value, value);
    }

    template <typename... SynchronizedValues>
        requires(is_type_instance_of_v<std::remove_cvref_t<SynchronizedValues>, synchronized_value> and ...)
    auto synchronize(SynchronizedValues&&... sync_values)
    {
        return std::make_tuple(std::forward<SynchronizedValues>(sync_values).synchronize()...);
    }
}

#endif
