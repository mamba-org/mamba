// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <utility>

#include <catch2/catch_all.hpp>
#include <fmt/format.h>

namespace Catch
{
    template <typename T, typename U>
    struct StringMaker<std::pair<T, U>>
    {
        static std::string convert(const std::pair<T, U>& value)
        {
            return fmt::format("std::pair{{{}, {}}}", value.first, value.second);
        }
    };
}
