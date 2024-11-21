// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <optional>
#include <string>

#include <catch2/catch_tostring.hpp>
#include <fmt/format.h>

namespace Catch
{
    template <typename T>
    struct StringMaker<std::optional<T>>
    {
        static std::string convert(const std::optional<T>& value)
        {
            if (value.has_value())
            {
                return fmt::format("Some({})", value.value());
            }
            return "None";
        }
    };
}
