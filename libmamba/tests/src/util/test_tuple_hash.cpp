// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <tuple>

#include <doctest/doctest.h>

#include "mamba/util/tuple_hash.hpp"

using namespace mamba::util;


TEST_SUITE("util::tuple_hash")
{
    TEST_CASE("hash_tuple")
    {
        const auto t1 = std::tuple{ 33, std::string("hello") };
        CHECK_NE(hash_tuple(t1), 0);

        // Hash colision are hard to predict, but this is so trivial it is likely a bug if it fails.
        const auto t2 = std::tuple{ 0, std::string("hello") };
        CHECK_NE(hash_tuple(t1), hash_tuple(t2));

        const auto t3 = std::tuple{ std::string("hello"), 33 };
        CHECK_NE(hash_tuple(t1), hash_tuple(t3));
    }
}
