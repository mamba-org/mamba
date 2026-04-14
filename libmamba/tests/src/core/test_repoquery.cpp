// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <vector>

#include <catch2/catch_all.hpp>

namespace mamba
{
    std::vector<std::string> extract_package_names_from_specs(const std::vector<std::string>& specs);
    std::vector<std::string>
    extract_exact_package_names_from_specs(const std::vector<std::string>& specs);
}

using namespace mamba;

TEST_CASE("repoquery sharded roots extraction", "[mamba::api][repoquery]")
{
    SECTION("extract names from valid MatchSpecs")
    {
        const std::vector<std::string> specs = {
            "xtensor=0.24.5",
            "conda-forge::python>=3.11",
            "xsimd[version='>=13']",
        };

        const auto names = extract_package_names_from_specs(specs);
        REQUIRE(names == std::vector<std::string>{ "xtensor", "python", "xsimd" });
    }

    SECTION("ignore name-free specs and keep valid names")
    {
        const std::vector<std::string> specs = {
            "numpy<2",
            ">=1.0",
        };

        const auto names = extract_package_names_from_specs(specs);
        REQUIRE(names == std::vector<std::string>{ "numpy" });
    }

    SECTION("empty when all specs are name-free")
    {
        const std::vector<std::string> specs = {
            ">=1.0",
            "<2",
        };

        const auto names = extract_package_names_from_specs(specs);
        REQUIRE(names.empty());
    }
}

TEST_CASE("repoquery search exact roots extraction", "[mamba::api][repoquery]")
{
    SECTION("extract exact names")
    {
        const std::vector<std::string> specs = {
            "xtensor",
            "python=3.12",
        };

        const auto names = extract_exact_package_names_from_specs(specs);
        REQUIRE(names == std::vector<std::string>{ "xtensor", "python" });
    }

    SECTION("reject glob queries")
    {
        const std::vector<std::string> specs = {
            "xtensor*",
        };

        const auto names = extract_exact_package_names_from_specs(specs);
        REQUIRE(names.empty());
    }

    SECTION("reject mixed exact and glob queries")
    {
        const std::vector<std::string> specs = {
            "xtensor",
            "xsimd*",
        };

        const auto names = extract_exact_package_names_from_specs(specs);
        REQUIRE(names.empty());
    }

    SECTION("skip invalid MatchSpec and keep exact names")
    {
        const std::vector<std::string> specs = {
            "xtensor",
            "python[version='>=3.11'",
            "xsimd=13",
        };

        const auto names = extract_exact_package_names_from_specs(specs);
        REQUIRE(names == std::vector<std::string>{ "xtensor", "xsimd" });
    }
}
