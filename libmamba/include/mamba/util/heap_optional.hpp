// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_HEAP_OPTIONAL_HPP
#define MAMBA_UTIL_HEAP_OPTIONAL_HPP

#include <memory>
#include <optional>
#include <utility>

namespace mamba::util
{
    /**
     * An optional akin to ``std::optional`` but uses heap-allocated storage.
     *
     * This is useful for large unlikely data, akin to ``std::unique_ptr`` but also
     * provides copy semantics.
     */
    template <typename T>
    class heap_optional
    {
    public:

        using element_type = T;
        using const_element_type = const element_type;
        using reference_type = element_type&;
        using const_reference_type = const_element_type&;
        using right_reference_type = element_type&&;
        using pointer_type = element_type*;
        using const_pointer_type = element_type*;

        heap_optional() = default;
        heap_optional(std::nullopt_t);
        heap_optional(const heap_optional&);
        heap_optional(heap_optional&&) noexcept = default;
        explicit heap_optional(element_type&& obj);
        explicit heap_optional(const element_type& obj);

        auto operator=(const heap_optional&) -> heap_optional&;
        auto operator=(heap_optional&&) noexcept -> heap_optional& = default;

        [[nodiscard]] auto get() noexcept -> pointer_type;
        [[nodiscard]] auto get() const noexcept -> const_pointer_type;

        [[nodiscard]] auto operator*() const -> const_reference_type;
        [[nodiscard]] auto operator*() -> reference_type;

        [[nodiscard]] auto operator->() const noexcept -> const_pointer_type;
        [[nodiscard]] auto operator->() noexcept -> pointer_type;

        [[nodiscard]] auto has_value() const noexcept -> bool;
        [[nodiscard]] explicit operator bool() const noexcept;

        [[nodiscard]] auto value() & -> reference_type;
        [[nodiscard]] auto value() const& -> const_reference_type;
        [[nodiscard]] auto value() && -> right_reference_type;

        template <typename U>
        [[nodiscard]] auto value_or(U&& other) const& -> element_type;
        template <typename U>
        [[nodiscard]] auto value_or(U&& other) && -> element_type;

        template <typename... Args>
        auto emplace(Args&&... args) -> reference_type;

        void reset();

        [[nodiscard]] auto operator==(const heap_optional& other) const -> bool
        {
            if (has_value() && other.has_value())
            {
                return *m_ptr == *other;
            }
            return !has_value() && !other.has_value();
        }

        [[nodiscard]] auto operator!=(const heap_optional& other) const -> bool
        {
            return !(*this == other);
        }

    private:

        std::unique_ptr<element_type> m_ptr = nullptr;
    };

    template <typename T>
    heap_optional(T&&) -> heap_optional<T>;
    template <typename T>
    heap_optional(const T&) -> heap_optional<T>;

    /*************************************
     *  Implementation of heap_optional  *
     *************************************/

    template <typename T>
    heap_optional<T>::heap_optional(std::nullopt_t)
    {
    }

    template <typename T>
    heap_optional<T>::heap_optional(element_type&& obj)
        : m_ptr(std::make_unique<element_type>(std::move(obj)))
    {
    }

    template <typename T>
    heap_optional<T>::heap_optional(const element_type& obj)
        : m_ptr(std::make_unique<element_type>(obj))
    {
    }

    template <typename T>
    heap_optional<T>::heap_optional(const heap_optional& other)
    {
        if (other.has_value())
        {
            m_ptr = std::make_unique<T>(*other);
        }
    }

    template <typename T>
    auto heap_optional<T>::operator=(const heap_optional& other) -> heap_optional&
    {
        if (other.has_value())
        {
            m_ptr = std::make_unique<T>(*other);
        }
        else
        {
            m_ptr = nullptr;
        }
        return *this;
    }

    template <typename T>
    auto heap_optional<T>::get() noexcept -> pointer_type
    {
        return m_ptr.get();
    }

    template <typename T>
    auto heap_optional<T>::get() const noexcept -> const_pointer_type
    {
        return m_ptr.get();
    }

    template <typename T>
    auto heap_optional<T>::operator*() const -> const_reference_type
    {
        return *m_ptr;
    }

    template <typename T>
    auto heap_optional<T>::operator*() -> reference_type
    {
        return *m_ptr;
    }

    template <typename T>
    auto heap_optional<T>::operator->() const noexcept -> const_pointer_type
    {
        return m_ptr.get();
    }

    template <typename T>
    auto heap_optional<T>::operator->() noexcept -> pointer_type
    {
        return m_ptr.get();
    }

    template <typename T>
    auto heap_optional<T>::has_value() const noexcept -> bool
    {
        return m_ptr != nullptr;
    }

    template <typename T>
    heap_optional<T>::operator bool() const noexcept
    {
        return has_value();
    }

    template <typename T>
    auto heap_optional<T>::value() & -> reference_type
    {
        if (has_value())
        {
            return *m_ptr;
        }
        throw std::bad_optional_access();
    }

    template <typename T>
    auto heap_optional<T>::value() const& -> const_reference_type
    {
        if (has_value())
        {
            return *m_ptr;
        }
        throw std::bad_optional_access();
    }

    template <typename T>
    auto heap_optional<T>::value() && -> right_reference_type
    {
        if (has_value())
        {
            return std::move(*m_ptr);
        }
        throw std::bad_optional_access();
    }

    template <typename T>
    template <typename U>
    auto heap_optional<T>::value_or(U&& other) const& -> element_type
    {
        if (has_value())
        {
            return *m_ptr;
        }
        return static_cast<element_type>(std::forward<U>(other));
    }

    template <typename T>
    template <typename U>
    auto heap_optional<T>::value_or(U&& other) && -> element_type
    {
        if (has_value())
        {
            return std::move(*m_ptr);
        }
        return static_cast<element_type>(std::forward<U>(other));
    }

    template <typename T>
    template <typename... Args>
    auto heap_optional<T>::emplace(Args&&... args) -> reference_type
    {
        m_ptr = std::make_unique<T>(std::forward<Args>(args)...);
        return *m_ptr;
    }

    template <typename T>
    void heap_optional<T>::reset()
    {
        m_ptr = nullptr;
    }
}

#endif
