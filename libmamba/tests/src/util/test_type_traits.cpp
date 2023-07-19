// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>

#include <doctest/doctest.h>

#include "mamba/util/type_traits.hpp"

using namespace mamba::util;


TEST_SUITE("type_traits")
{
    struct NotOStreamable
    {
    };

    struct OStreamable
    {
    };
    auto operator<<(std::ostream& s, const OStreamable&)->std::ostream&;

    TEST_CASE("ostreamable")
    {
        CHECK(is_ostreamable_v<int>);
        CHECK(is_ostreamable_v<std::string>);
        CHECK_FALSE(is_ostreamable_v<NotOStreamable>);
        CHECK(is_ostreamable_v<OStreamable>);
    }
}
