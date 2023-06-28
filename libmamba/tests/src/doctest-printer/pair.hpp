// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <utility>

#include <doctest/doctest.h>
#include <fmt/format.h>

namespace doctest
{
    template <typename T, typename U>
    struct StringMaker<std::pair<T, U>>
    {
        static auto convert(const std::pair<T, U>& value) -> String
        {
            return { fmt::format("std::pair{{{}, {}}}", value.first, value.second).c_str() };
        }
    };
}
