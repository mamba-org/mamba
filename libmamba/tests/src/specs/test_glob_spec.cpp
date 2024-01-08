// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/specs/glob_spec.hpp"

using namespace mamba::specs;

TEST_SUITE("specs::glob_spec")
{
    // See also test_parser for Glob matcher tests

    TEST_CASE("Free")
    {
        auto spec = GlobSpec();

        CHECK(spec.contains(""));
        CHECK(spec.contains("hello"));

        CHECK_EQ(spec.str(), "*");
        CHECK(spec.is_free());
        CHECK_FALSE(spec.is_exact());
    }

    TEST_CASE("*mkl*")
    {
        auto spec = GlobSpec("mkl");

        CHECK(spec.contains("mkl"));
        CHECK_FALSE(spec.contains(""));
        CHECK_FALSE(spec.contains("nomkl"));
        CHECK_FALSE(spec.contains("hello"));

        CHECK_EQ(spec.str(), "mkl");
        CHECK_FALSE(spec.is_free());
        CHECK(spec.is_exact());
    }

    TEST_CASE("*py*")
    {
        // See also test_parser for Glob matcher tests
        auto spec = GlobSpec("*py*");

        CHECK(spec.contains("py"));
        CHECK(spec.contains("pypy"));
        CHECK(spec.contains("cpython-linux-64"));
        CHECK_FALSE(spec.contains("rust"));
        CHECK_FALSE(spec.contains("hello"));

        CHECK_EQ(spec.str(), "*py*");
        CHECK_FALSE(spec.is_free());
        CHECK_FALSE(spec.is_exact());
    }
}
