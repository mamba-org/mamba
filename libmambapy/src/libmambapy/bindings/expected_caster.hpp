// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_PY_EXPECTED_CASTER
#define MAMBA_PY_EXPECTED_CASTER

#include <exception>
#include <type_traits>
#include <utility>
#include <variant>

#include <pybind11/cast.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <tl/expected.hpp>

namespace PYBIND11_NAMESPACE
{
    namespace detail
    {

        /**
         * A caster for tl::expected that throws on unexpected.
         */
        template <typename T, typename E>
        struct type_caster<tl::expected<T, E>>
        {
            using value_type = T;

            auto load(handle src, bool convert) -> bool
            {
                auto caster = make_caster<T>();
                if (caster.load(src, convert))
                {
                    value = cast_op<T>(std::move(caster));
                    return true;
                }
                return false;
            }

            template <typename Expected>
            static auto cast(Expected&& src, return_value_policy policy, handle parent) -> handle
            {
                if (src)
                {
                    return make_caster<T>::cast(std::forward<Expected>(src).value(), policy, parent);
                }
                else
                {
                    // If we use ``expected`` without exception in our API, we need to convert them
                    // to an exception before throwing it in PyBind11 code.
                    // This could be done with partial specialization of this ``type_caster``.
                    static_assert(std::is_base_of_v<std::exception, E>);
                    throw std::forward<Expected>(src).error();
                }
            }

            PYBIND11_TYPE_CASTER(value_type, make_caster<T>::name);
        };

        template <typename E>
        struct type_caster<tl::expected<void, E>>
        {
            using value_type = std::nullptr_t;

            auto load(handle, bool) -> bool
            {
                return true;
            }

            template <typename Expected>
            static auto cast(Expected&& src, return_value_policy, handle) -> handle
            {
                if (src)
                {
                    return none();
                }
                else
                {
                    // If we use ``expected`` without exception in our API, we need to convert them
                    // to an exception before throwing it in PyBind11 code.
                    // This could be done with partial specialization of this ``type_caster``.
                    static_assert(std::is_base_of_v<std::exception, E>);
                    throw std::forward<Expected>(src).error();
                }
            }

            PYBIND11_TYPE_CASTER(value_type, const_name("None"));

        /**
         * A caster for std::variant with reference_wrapper types.
         */
        template <typename... Types>
        struct type_caster<std::variant<std::reference_wrapper<Types>...>>
        {
            using value_type = std::variant<std::reference_wrapper<Types>...>;

            bool load(handle src, bool convert)
            {
                return false;  // We don't support loading from Python
            }

            static handle cast(const value_type& src, return_value_policy policy, handle parent)
            {
                return std::visit(
                    [policy, parent](const auto& v) -> handle
                    {
                        using T = std::decay_t<decltype(v.get())>;
                        return make_caster<T>::cast(v.get(), policy, parent);
                    },
                    src
                );
            }

            PYBIND11_TYPE_CASTER(value_type, _("variant"));
        };

        /**
         * A caster for std::reference_wrapper.
         */
        template <typename T>
        struct type_caster<std::reference_wrapper<T>>
        {
            using value_type = std::reference_wrapper<T>;

            bool load(handle src, bool convert)
            {
                return false;  // We don't support loading from Python
            }

            static handle cast(const value_type& src, return_value_policy policy, handle parent)
            {
                return make_caster<T>::cast(src.get(), policy, parent);
            }

            PYBIND11_TYPE_CASTER(value_type, _("reference_wrapper"));
        };

        /**
         * Specialization for the specific variant type used in the database.
         */
        template <>
        struct type_caster<std::variant<
            std::reference_wrapper<mamba::solver::libsolv::Database>,
            std::reference_wrapper<mamba::solver::resolvo::Database>>>
        {
            using value_type = std::variant<
                std::reference_wrapper<mamba::solver::libsolv::Database>,
                std::reference_wrapper<mamba::solver::resolvo::Database>>;

            bool load(handle src, bool convert)
            {
                return false;  // We don't support loading from Python
            }

            static handle cast(const value_type& src, return_value_policy policy, handle parent)
            {
                return std::visit(
                    [policy, parent](const auto& v) -> handle
                    {
                        using T = std::decay_t<decltype(v.get())>;
                        return make_caster<T>::cast(v.get(), policy, parent);
                    },
                    src
                );
            }

            PYBIND11_TYPE_CASTER(value_type, _("variant"));
        };

        /**
         * A caster for mamba::Context.
         */
        template <>
        struct type_caster<mamba::Context>
        {
            using value_type = mamba::Context;

            bool load(handle src, bool convert)
            {
                return false;  // We don't support loading from Python
            }

            static handle cast(const value_type& src, return_value_policy policy, handle parent)
            {
                return make_caster<value_type>::cast(src, policy, parent);
            }

            PYBIND11_TYPE_CASTER(value_type, _("Context"));
        };

        /**
         * A caster for mamba::PrefixData.
         */
        template <>
        struct type_caster<mamba::PrefixData>
        {
            using value_type = mamba::PrefixData;

            bool load(handle src, bool convert)
            {
                return false;  // We don't support loading from Python
            }

            static handle cast(const value_type& src, return_value_policy policy, handle parent)
            {
                return make_caster<value_type>::cast(src, policy, parent);
            }

            PYBIND11_TYPE_CASTER(value_type, _("PrefixData"));
        };
    }
}
#endif
