// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <string>

#include <catch2/catch_all.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

namespace Catch
{
    template <typename T, std::size_t N>
    struct StringMaker<std::array<T, N>>
    {
        static std::string convert(const std::array<T, N>& value)
        {
            return fmt::format("std::array{{{}}}", fmt::join(value, ", "));
        }
    };
}
