// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/specs/chimera_string_spec.hpp"

using namespace mamba::specs;

TEST_SUITE("specs::chimera_string_spec")
{
    TEST_CASE("Free")
    {
        auto spec = ChimeraStringSpec();

        CHECK(spec.contains(""));
        CHECK(spec.contains("hello"));

        CHECK_EQ(spec.str(), "*");
        CHECK(spec.is_explicitly_free());
        CHECK_FALSE(spec.is_exact());
        CHECK(spec.is_glob());
    }

    TEST_CASE("mkl")
    {
        auto spec = ChimeraStringSpec::parse("mkl").value();

        CHECK(spec.contains("mkl"));
        CHECK_FALSE(spec.contains(""));
        CHECK_FALSE(spec.contains("nomkl"));
        CHECK_FALSE(spec.contains("hello"));

        CHECK_EQ(spec.str(), "mkl");
        CHECK_FALSE(spec.is_explicitly_free());
        CHECK(spec.is_exact());
        CHECK(spec.is_glob());
    }

    TEST_CASE("py.*")
    {
        auto spec = ChimeraStringSpec::parse("py.*").value();

        CHECK(spec.contains("python"));
        CHECK(spec.contains("py"));
        CHECK(spec.contains("pypy"));
        CHECK_FALSE(spec.contains(""));
        CHECK_FALSE(spec.contains("cpython"));

        CHECK_EQ(spec.str(), "^py.*$");
        CHECK_FALSE(spec.is_explicitly_free());
        CHECK_FALSE(spec.is_exact());
        CHECK_FALSE(spec.is_glob());
    }

    TEST_CASE("^.*(accelerate|mkl)$")
    {
        auto spec = ChimeraStringSpec::parse("^.*(accelerate|mkl)$").value();

        CHECK(spec.contains("accelerate"));
        CHECK(spec.contains("mkl"));
        CHECK_FALSE(spec.contains(""));
        CHECK_FALSE(spec.contains("openblas"));

        CHECK_EQ(spec.str(), "^.*(accelerate|mkl)$");
        CHECK_FALSE(spec.is_explicitly_free());
        CHECK_FALSE(spec.is_exact());
        CHECK_FALSE(spec.is_glob());
    }

    TEST_CASE("Comparability and hashability")
    {
        auto spec1 = ChimeraStringSpec::parse("mkl").value();
        auto spec2 = ChimeraStringSpec::parse("mkl").value();
        auto spec3 = ChimeraStringSpec::parse("*").value();

        CHECK_EQ(spec1, spec2);
        CHECK_NE(spec1, spec3);

        std::hash<ChimeraStringSpec> hash_fn;
        CHECK_EQ(hash_fn(spec1), hash_fn(spec2));
        CHECK_NE(hash_fn(spec1), hash_fn(spec3));
    }
}
