// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>

#include <catch2/catch_all.hpp>

#include "mamba/util/type_traits.hpp"

using namespace mamba::util;

struct NotOStreamable
{
};

struct OStreamable
{
};

auto
operator<<(std::ostream& s, const OStreamable&) -> std::ostream&;

TEST_CASE("ostreamable")
{
    REQUIRE(is_ostreamable_v<int>);
    REQUIRE(is_ostreamable_v<std::string>);
    REQUIRE_FALSE(is_ostreamable_v<NotOStreamable>);
    REQUIRE(is_ostreamable_v<OStreamable>);
}
