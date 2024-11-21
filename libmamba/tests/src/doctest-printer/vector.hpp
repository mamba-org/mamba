// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <vector>

#include <catch2/catch_all.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

namespace Catch
{
    template <typename T, typename A>
    struct StringMaker<std::vector<T, A>>
    {
        static std::string convert(const std::vector<T, A>& value)
        {
            return fmt::format("std::vector{{{}}}", fmt::join(value, ", "));
        }
    };
}
