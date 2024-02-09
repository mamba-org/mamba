// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <utility>

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
                    throw std::forward<Expected>(src).error();
                }
            }

            PYBIND11_TYPE_CASTER(value_type, detail::concat(make_caster<T>::name, make_caster<E>::name));
        };
    }
}
#endif
