// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/specs/regex_spec.hpp"

using namespace mamba::specs;

namespace
{
    TEST_CASE("RegexSpec Free")
    {
        auto spec = RegexSpec();

        REQUIRE(spec.contains(""));
        REQUIRE(spec.contains("hello"));

        REQUIRE(spec.str() == "^.*$");
        REQUIRE(spec.is_explicitly_free());
        REQUIRE_FALSE(spec.is_exact());
    }

    TEST_CASE("RegexSpec mkl")
    {
        auto spec = RegexSpec::parse("mkl").value();

        REQUIRE(spec.contains("mkl"));
        REQUIRE_FALSE(spec.contains(""));
        REQUIRE_FALSE(spec.contains("nomkl"));
        REQUIRE_FALSE(spec.contains("hello"));

        REQUIRE(spec.str() == "^mkl$");
        REQUIRE_FALSE(spec.is_explicitly_free());
        REQUIRE(spec.is_exact());
    }

    TEST_CASE("RegexSpec py.*")
    {
        auto spec = RegexSpec::parse("py.*").value();

        REQUIRE(spec.contains("python"));
        REQUIRE(spec.contains("py"));
        REQUIRE(spec.contains("pypy"));
        REQUIRE_FALSE(spec.contains(""));
        REQUIRE_FALSE(spec.contains("cpython"));

        REQUIRE(spec.str() == "^py.*$");
        REQUIRE_FALSE(spec.is_explicitly_free());
        REQUIRE_FALSE(spec.is_exact());
    }

    TEST_CASE("RegexSpec ^.*(accelerate|mkl)$")
    {
        auto spec = RegexSpec::parse("^.*(accelerate|mkl)$").value();

        REQUIRE(spec.contains("accelerate"));
        REQUIRE(spec.contains("mkl"));
        REQUIRE_FALSE(spec.contains(""));
        REQUIRE_FALSE(spec.contains("openblas"));

        REQUIRE(spec.str() == "^.*(accelerate|mkl)$");
        REQUIRE_FALSE(spec.is_explicitly_free());
        REQUIRE_FALSE(spec.is_exact());
    }

    TEST_CASE("RegexSpec Comparability and hashability")
    {
        auto spec1 = RegexSpec::parse("pyth*").value();
        auto spec2 = RegexSpec::parse("pyth*").value();
        auto spec3 = RegexSpec::parse("python").value();

        REQUIRE(spec1 == spec2);
        REQUIRE(spec1 != spec3);

        auto hash_fn = std::hash<RegexSpec>();
        REQUIRE(hash_fn(spec1) == hash_fn(spec2));
        REQUIRE(hash_fn(spec1) != hash_fn(spec3));
    }

    TEST_CASE("RegexSpec py3.10_cuda11.8*")
    {
        auto spec = RegexSpec::parse("py3.10_cuda11.8*").value();
        REQUIRE(spec.contains("py3.10_cuda11.8_cudnn8.7.0_0"));
    }

    TEST_CASE("RegexSpec * semantic")
    {
        auto spec = RegexSpec::parse("py3.*").value();

        REQUIRE(spec.contains("py3."));
        REQUIRE(spec.contains("py3.10"));
        REQUIRE(spec.contains("py3.10_cuda11.8_cudnn8.7.0_0"));
    }

}
