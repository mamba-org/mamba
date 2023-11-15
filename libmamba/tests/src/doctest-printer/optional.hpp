// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <optional>

#include <doctest/doctest.h>
#include <fmt/format.h>

namespace doctest
{
    template <typename T>
    struct StringMaker<std::optional<T>>
    {
        static auto convert(const std::optional<T>& value) -> String
        {
            if (value.has_value())
            {
                return { fmt::format("Some({})", value.value()).c_str() };
            }
            return "None";
        }
    };
}
