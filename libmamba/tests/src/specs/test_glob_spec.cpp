// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/specs/glob_spec.hpp"

using namespace mamba::specs;

namespace
{
    // See also test_parser for Glob matcher tests

    TEST_CASE("Free")
    {
        auto spec = GlobSpec();

        REQUIRE(spec.contains(""));
        REQUIRE(spec.contains("hello"));

        CHECK_EQ(spec.str(), "*");
        REQUIRE(spec.is_free());
        REQUIRE_FALSE(spec.is_exact());
    }

    TEST_CASE("*mkl*")
    {
        auto spec = GlobSpec("mkl");

        REQUIRE(spec.contains("mkl"));
        REQUIRE_FALSE(spec.contains(""));
        REQUIRE_FALSE(spec.contains("nomkl"));
        REQUIRE_FALSE(spec.contains("hello"));

        CHECK_EQ(spec.str(), "mkl");
        REQUIRE_FALSE(spec.is_free());
        REQUIRE(spec.is_exact());
    }

    TEST_CASE("*py*")
    {
        // See also test_parser for Glob matcher tests
        auto spec = GlobSpec("*py*");

        REQUIRE(spec.contains("py"));
        REQUIRE(spec.contains("pypy"));
        REQUIRE(spec.contains("cpython-linux-64"));
        REQUIRE_FALSE(spec.contains("rust"));
        REQUIRE_FALSE(spec.contains("hello"));

        CHECK_EQ(spec.str(), "*py*");
        REQUIRE_FALSE(spec.is_free());
        REQUIRE_FALSE(spec.is_exact());
    }

    TEST_CASE("Comparability and hashability")
    {
        auto spec1 = GlobSpec("py*");
        auto spec2 = GlobSpec("py*");
        auto spec3 = GlobSpec("pyth*");

        CHECK_EQ(spec1, spec2);
        CHECK_NE(spec1, spec3);

        auto hash_fn = std::hash<GlobSpec>();
        REQUIRE(hash_fn(spec1) == hash_fn(spec2);
        REQUIRE(hash_fn(spec1) != hash_fn(spec3);
    }
}
