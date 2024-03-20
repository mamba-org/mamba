// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/specs/regex_spec.hpp"

using namespace mamba::specs;

TEST_SUITE("specs::regex_spec")
{
    TEST_CASE("Free")
    {
        auto spec = RegexSpec();

        CHECK(spec.contains(""));
        CHECK(spec.contains("hello"));

        CHECK_EQ(spec.str(), "^.*$");
        CHECK(spec.is_explicitly_free());
        CHECK_FALSE(spec.is_exact());
    }

    TEST_CASE("mkl")
    {
        auto spec = RegexSpec::parse("mkl").value();

        CHECK(spec.contains("mkl"));
        CHECK_FALSE(spec.contains(""));
        CHECK_FALSE(spec.contains("nomkl"));
        CHECK_FALSE(spec.contains("hello"));

        CHECK_EQ(spec.str(), "^mkl$");
        CHECK_FALSE(spec.is_explicitly_free());
        CHECK(spec.is_exact());
    }

    TEST_CASE("py.*")
    {
        auto spec = RegexSpec::parse("py.*").value();

        CHECK(spec.contains("python"));
        CHECK(spec.contains("py"));
        CHECK(spec.contains("pypy"));
        CHECK_FALSE(spec.contains(""));
        CHECK_FALSE(spec.contains("cpython"));

        CHECK_EQ(spec.str(), "^py.*$");
        CHECK_FALSE(spec.is_explicitly_free());
        CHECK_FALSE(spec.is_exact());
    }

    TEST_CASE("^.*(accelerate|mkl)$")
    {
        auto spec = RegexSpec::parse("^.*(accelerate|mkl)$").value();

        CHECK(spec.contains("accelerate"));
        CHECK(spec.contains("mkl"));
        CHECK_FALSE(spec.contains(""));
        CHECK_FALSE(spec.contains("openblas"));

        CHECK_EQ(spec.str(), "^.*(accelerate|mkl)$");
        CHECK_FALSE(spec.is_explicitly_free());
        CHECK_FALSE(spec.is_exact());
    }
}
