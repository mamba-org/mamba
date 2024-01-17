// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <type_traits>
#include <utility>
#include <variant>

#include <pybind11/cast.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <tl/expected.hpp>

#ifndef MAMBA_PY_EXPECTED_CASTER
#define MAMBA_PY_EXPECTED_CASTER

namespace PYBIND11_NAMESPACE
{
    namespace detail
    {
        namespace
        {
            template <
                typename Expected,
                typename T = typename Expected::value_type,
                typename E = typename Expected::error_type>
            auto expected_to_variant(Expected&& expected) -> std::variant<T, E>
            {
                if (expected)
                {
                    return { std::forward<Expected>(expected).value() };
                }
                return { std::forward<Expected>(expected).error() };
            }

            template <
                typename Variant,
                typename T = std::decay_t<decltype(std::get<0>(std::declval<Variant>()))>,
                typename E = std::decay_t<decltype(std::get<1>(std::declval<Variant>()))>>
            auto expected_to_variant(Variant&& var) -> tl::expected<T, E>
            {
                static_assert(std::variant_size_v<Variant> == 2);
                return std::visit(
                    [](auto&& v) -> tl::expected<T, E> { return { std::forward<deltype(v)>(v) }; },
                    var
                );
            }
        }

        /**
         * A caster for tl::expected that converts to a union.
         *
         * The caster works by converting to a the expected to a variant and then calls the
         * variant caster.
         *
         * A future direction could be considered to wrap the union into a Python "Expected",
         * with methods such as ``and_then``, ``or_else``, and thowing method like ``value``
         * and ``error``.
         */
        template <typename T, typename E>
        struct type_caster<tl::expected<T, E>>
        {
            using value_type = tl::expected<T, E>;
            using variant_type = std::variant<T, E>;
            using caster_type = std::decay_t<decltype(make_caster<variant_type>())>;

            auto load(handle src, bool convert) -> bool
            {
                auto caster = make_caster<variant_type>();
                if (caster.load(src, convert))
                {
                    value = variant_to_expected(cast_op<variant_type>(std::move(caster)));
                }
                return false;
            }

            template <typename Expected>
            static auto cast(Expected&& src, return_value_policy policy, handle parent) -> handle
            {
                return caster_type::cast(expected_to_variant(std::forward<Expected>(src)), policy, parent);
            }

            PYBIND11_TYPE_CASTER(
                value_type,
                const_name(R"(Union[)") + detail::concat(make_caster<T>::name, make_caster<E>::name)
                    + const_name(R"(])")
            );
        };
    }
}
#endif
