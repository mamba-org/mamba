// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/specs/conda_url.hpp"

namespace doctest
{
    template <>
    struct StringMaker<mamba::specs::CondaURL>
    {
        static auto convert(const mamba::specs::CondaURL& value) -> String
        {
            return { value.str().c_str() };
        }
    };
}
