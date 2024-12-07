// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/specs/chimera_string_spec.hpp"

using namespace mamba::specs;

TEST_CASE("ChimeraStringSpec Free")
{
    auto spec = ChimeraStringSpec();

    REQUIRE(spec.contains(""));
    REQUIRE(spec.contains("hello"));

    REQUIRE(spec.str() == "*");
    REQUIRE(spec.is_explicitly_free());
    REQUIRE_FALSE(spec.is_exact());
    REQUIRE(spec.is_glob());
}

TEST_CASE("ChimeraStringSpec mkl")
{
    auto spec = ChimeraStringSpec::parse("mkl").value();

    REQUIRE(spec.contains("mkl"));
    REQUIRE_FALSE(spec.contains(""));
    REQUIRE_FALSE(spec.contains("nomkl"));
    REQUIRE_FALSE(spec.contains("hello"));

    REQUIRE(spec.str() == "mkl");
    REQUIRE_FALSE(spec.is_explicitly_free());
    REQUIRE(spec.is_exact());
    REQUIRE(spec.is_glob());
}

TEST_CASE("ChimeraStringSpec py.*")
{
    auto spec = ChimeraStringSpec::parse("py.*").value();

    REQUIRE(spec.contains("python"));
    REQUIRE(spec.contains("py"));
    REQUIRE(spec.contains("pypy"));
    REQUIRE_FALSE(spec.contains(""));
    REQUIRE_FALSE(spec.contains("cpython"));

    REQUIRE(spec.str() == "^py.*$");
    REQUIRE_FALSE(spec.is_explicitly_free());
    REQUIRE_FALSE(spec.is_exact());
    REQUIRE_FALSE(spec.is_glob());
}

TEST_CASE("ChimeraStringSpec ^.*(accelerate|mkl)$")
{
    auto spec = ChimeraStringSpec::parse("^.*(accelerate|mkl)$").value();

    REQUIRE(spec.contains("accelerate"));
    REQUIRE(spec.contains("mkl"));
    REQUIRE_FALSE(spec.contains(""));
    REQUIRE_FALSE(spec.contains("openblas"));

    REQUIRE(spec.str() == "^.*(accelerate|mkl)$");
    REQUIRE_FALSE(spec.is_explicitly_free());
    REQUIRE_FALSE(spec.is_exact());
    REQUIRE_FALSE(spec.is_glob());
}

TEST_CASE("ChimeraStringSpec Comparability and hashability")
{
    auto spec1 = ChimeraStringSpec::parse("mkl").value();
    auto spec2 = ChimeraStringSpec::parse("mkl").value();
    auto spec3 = ChimeraStringSpec::parse("*").value();

    REQUIRE(spec1 == spec2);
    REQUIRE(spec1 != spec3);

    std::hash<ChimeraStringSpec> hash_fn;
    REQUIRE(hash_fn(spec1) == hash_fn(spec2));
    REQUIRE(hash_fn(spec1) != hash_fn(spec3));
}
