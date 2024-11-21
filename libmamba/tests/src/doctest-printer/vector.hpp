// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <vector>

#include <catch2/catch_all.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

namespace doctest
{
    template <typename T, typename A>
    struct StringMaker<std::vector<T, A>>
    {
        static auto convert(const std::vector<T, A>& value) -> String
        {
            return { fmt::format("std::vector{{{}}}", fmt::join(value, ", ")).c_str() };
        }
    };
}
