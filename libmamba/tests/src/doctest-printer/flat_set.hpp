// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>

#include <catch2/catch_all.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include "mamba/util/flat_set.hpp"

namespace Catch
{
    template <typename K, typename C, typename A>
    struct StringMaker<mamba::util::flat_set<K, C, A>>
    {
        static std::string convert(const mamba::util::flat_set<K, C, A>& value)
        {
            return fmt::format("mamba::util::flat_set{{{}}}", fmt::join(value, ", "));
        }
    };
}
