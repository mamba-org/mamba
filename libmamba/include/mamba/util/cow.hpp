// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_COW_HPP
#define MAMBA_UTIL_COW_HPP

#include <string>
#include <string_view>
#include <variant>

namespace mamba::util
{
    /**
     * A copy on write class.
     *
     * It contains either a view or a value.
     * This is useful as a return type of functions that may modify their input but usually do not.
     *
     * The type is kept explicit, no conversion, to avoid hard to track dead references.
     */
    template <typename Owned, typename ConstBorrowed>
    class cow
    {
    public:

        using owned_type = Owned;
        using const_borrowed_type = ConstBorrowed;

        static auto make_owned(owned_type&& val) -> cow;
        static auto make_owned(const_borrowed_type val) -> cow;

        static auto make_borrowed(const owned_type& val) -> cow;
        static auto make_borrowed(const_borrowed_type val) -> cow;

        explicit cow(owned_type&& val);
        explicit cow(const owned_type& val);
        explicit cow(const_borrowed_type val);

        [[nodiscard]] auto is_owned() const -> bool;
        [[nodiscard]] auto is_borrowed() const -> bool;

        /**
         * Return a view unconditionally.
         *
         * The view is either to the borrowed type or to the owned value.
         */
        [[nodiscard]] auto view() const -> const_borrowed_type;

        /**
         * Return a copy of the value unconditionally.
         *
         * The value is created either form the borrowed or owned value.
         */
        [[nodiscard]] auto value() const& -> owned_type;

        /**
         * Return a value unconditionally.
         *
         * The value is created either form copying the borrowed value or moving the owned value.
         */
        [[nodiscard]] auto value() && -> owned_type;

    private:

        std::variant<const_borrowed_type, owned_type> m_value;
    };

    template <class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
    using basic_string_cow = cow<
        std::basic_string<CharT, Traits, Allocator>,
        std::basic_string_view<CharT, Traits>>;

    using string_cow = cow<std::string, std::string_view>;
    using wstring_cow = cow<std::wstring, std::wstring_view>;

    /***************************
     *  Implementation of cow  *
     ***************************/

    template <typename O, typename B>
    auto cow<O, B>::make_owned(owned_type&& val) -> cow
    {
        return cow(std::move(val));
    }

    template <typename O, typename B>
    auto cow<O, B>::make_owned(const_borrowed_type val) -> cow
    {
        return cow(owned_type(val));
    }

    template <typename O, typename B>
    auto cow<O, B>::make_borrowed(const owned_type& val) -> cow
    {
        return cow(const_borrowed_type(val));
    }

    template <typename O, typename B>
    auto cow<O, B>::make_borrowed(const_borrowed_type val) -> cow
    {
        return cow(val);
    }

    template <typename O, typename B>
    cow<O, B>::cow(owned_type&& val)
        : m_value(std::move(val))
    {
    }

    template <typename O, typename B>
    cow<O, B>::cow(const owned_type& val)
        : m_value(const_borrowed_type(val))

    {
    }

    template <typename O, typename B>
    cow<O, B>::cow(const_borrowed_type val)
        : m_value(val)
    {
    }

    template <typename O, typename B>
    auto cow<O, B>::is_owned() const -> bool
    {
        return std::holds_alternative<owned_type>(m_value);
    }

    template <typename O, typename B>
    auto cow<O, B>::is_borrowed() const -> bool
    {
        return std::holds_alternative<const_borrowed_type>(m_value);
    }

    template <typename O, typename B>
    auto cow<O, B>::view() const -> const_borrowed_type
    {
        return std::visit([](const auto& val) { return const_borrowed_type(val); }, m_value);
    }

    template <typename O, typename B>
    auto cow<O, B>::value() const& -> owned_type
    {
        return std::visit([](const auto& val) { return owned_type(val); }, m_value);
    }

    template <typename O, typename B>
    auto cow<O, B>::value() && -> owned_type
    {
        return std::visit(
            [](auto&& val) { return owned_type(std::forward<delctype(val)>(val)); },
            std::move(m_value)
        );
    }
}
#endif
