// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_METAPROG_HPP
#define MAMBA_UTIL_METAPROG_HPP


namespace mamba::util
{
    /////////////////////////////
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

}

#endif
