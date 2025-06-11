// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

// Private libmamba header
#include "core/transaction_context.hpp"

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
            auto path_empty_ver = get_python_short_path("").string();
            auto path = get_python_short_path("3.5.0").string();
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
            REQUIRE(get_python_site_packages_short_path("").string() == "");

            auto path = get_python_site_packages_short_path("3.5.0");
            auto path_str = path.string();
            auto path_gen_str = path.generic_string();
#ifdef _WIN32
            REQUIRE(path_str == "Lib\\site-packages");
            REQUIRE(path_gen_str == "Lib/site-packages");
#else
            REQUIRE(path_str == "lib/python3.5.0/site-packages");
            REQUIRE(path_gen_str == "lib/python3.5.0/site-packages");
#endif
        }

        TEST_CASE("get_bin_directory_short_path")
        {
            auto path = get_bin_directory_short_path().string();
#ifdef _WIN32
            REQUIRE(path == "Scripts");
#else
            REQUIRE(path == "bin");
#endif
        }

        TEST_CASE("get_python_noarch_target_path")
        {
            auto random_path = get_python_noarch_target_path("some_lib/some_folder", "bla");
            auto random_path_str = random_path.string();
            auto random_path_gen_str = random_path.generic_string();

            auto sp_path = get_python_noarch_target_path(
                "site-packages/some_random_package",
                "target_site_packages_short_path"
            );
            auto sp_path_str = sp_path.string();
            auto sp_path_gen_str = sp_path.generic_string();

            auto ps_path = get_python_noarch_target_path(
                "python-scripts/some_random_file",
                "target_site_packages_short_path"
            );
            auto ps_path_str = ps_path.string();
            auto ps_path_gen_str = ps_path.generic_string();

            REQUIRE(random_path_gen_str == "some_lib/some_folder");
            REQUIRE(sp_path_gen_str == "target_site_packages_short_path/some_random_package");
#ifdef _WIN32
            REQUIRE(random_path_str == "some_lib\\some_folder");
            REQUIRE(sp_path_str == "target_site_packages_short_path\\some_random_package");
            REQUIRE(ps_path_str == "Scripts\\some_random_file");
            REQUIRE(ps_path_gen_str == "Scripts/some_random_file");
#else
            REQUIRE(random_path_str == "some_lib/some_folder");
            REQUIRE(sp_path_str == "target_site_packages_short_path/some_random_package");
            REQUIRE(ps_path_str == "bin/some_random_file");
            REQUIRE(ps_path_gen_str == "bin/some_random_file");
#endif
        }
    }
}  // namespace mamba
