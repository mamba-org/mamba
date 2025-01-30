// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/core/transaction_context.hpp"

namespace mamba
{

    namespace
    {
        TEST_CASE("compute_short_python_version")
        {
            REQUIRE(compute_short_python_version("") == "");
            REQUIRE(compute_short_python_version("3.5") == "3.5");
            REQUIRE(compute_short_python_version("3.5.0") == "3.5");
        }

        TEST_CASE("get_python_short_path")
        {
            auto path_empty_ver = get_python_short_path("");
            auto path = get_python_short_path("3.5.0");
#ifdef _WIN32
            REQUIRE(path_empty_ver == "python.exe");
            REQUIRE(path == "python.exe");
#else
            REQUIRE(path_empty_ver == "bin/python");
            REQUIRE(path == "bin/python3.5.0");
#endif
        }

        TEST_CASE("get_python_site_packages_short_path")
        {
            REQUIRE(get_python_site_packages_short_path("") == "");

            auto path = get_python_site_packages_short_path("3.5.0");
#ifdef _WIN32
            REQUIRE(path == "Lib/site-packages");
#else
            REQUIRE(path == "lib/python3.5.0/site-packages");
#endif
        }

        TEST_CASE("get_bin_directory_short_path")
        {
            auto path = get_bin_directory_short_path();
#ifdef _WIN32
            REQUIRE(path == "Scripts");
#else
            REQUIRE(path == "bin");
#endif
        }

        TEST_CASE("get_python_noarch_target_path")
        {
            REQUIRE(get_python_noarch_target_path("lib/site-packages", "bla") == "lib/site-packages");
            REQUIRE(
                get_python_noarch_target_path(
                    "site-packages/some_random_package",
                    "target_site_packages_short_path"
                )
                == "target_site_packages_short_path/some_random_package"
            );

            auto path = get_python_noarch_target_path(
                "python-scripts/some_random_file",
                "target_site_packages_short_path"
            );
#ifdef _WIN32
            REQUIRE(path == "Scripts/some_random_file");
#else
            REQUIRE(path == "bin/some_random_file");
#endif
        }
    }

}  // namespace mamba
