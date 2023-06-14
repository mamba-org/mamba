// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>

#include <doctest/doctest.h>
#include <fmt/format.h>

namespace doctest
{
    template <typename T, std::size_t N>
    struct StringMaker<std::array<T, N>>
    {
        static auto convert(const std::array<T, N>& value) -> String
        {
            return { fmt::format("std::array{{{}}}", fmt::join(value, ", ")).c_str() };
        }
    };
}
