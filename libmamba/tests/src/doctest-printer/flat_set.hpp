// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>
#include <fmt/format.h>

#include "mamba/util/flat_set.hpp"

namespace doctest
{
    template <typename K, typename C, typename A>
    struct StringMaker<mamba::util::flat_set<K, C, A>>
    {
        static auto convert(const mamba::util::flat_set<K, C, A>& value) -> String
        {
            return { fmt::format("std::flat_set{{{}}}", fmt::join(value, ", ")).c_str() };
        }
    };
}
