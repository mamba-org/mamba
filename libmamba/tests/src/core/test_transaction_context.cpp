// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

// Private libmamba header
#include "core/link.hpp"
// Private libmamba header
#include "mamba/core/context_params.hpp"
#include "mamba/core/util.hpp"
#include "mamba/specs/package_info.hpp"

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

        TEST_CASE("effective_python_site_packages_path")
        {
            auto python_pkg = specs::PackageInfo("python");
            python_pkg.version = "3.13.1";
            python_pkg.build_string = "h9a34b6e_5_cp313t";

            SECTION("freethreaded site-packages path")
            {
                python_pkg.python_site_packages_path = "lib/python3.13t/site-packages";

#ifdef _WIN32
                const auto expected = std::string("Lib/site-packages");
#else
                const auto expected = std::string("lib/python3.13t/site-packages");
#endif
                REQUIRE(effective_python_site_packages_path(python_pkg) == expected);
            }

            SECTION("rewrites standard path for freethreaded builds")
            {
                python_pkg.python_site_packages_path =
#ifdef _WIN32
                    "Lib/site-packages";
#else
                    "lib/python3.13/site-packages";
#endif

#ifdef _WIN32
                const auto expected = std::string("Lib/site-packages");
#else
                const auto expected = std::string("lib/python3.13t/site-packages");
#endif
                REQUIRE(effective_python_site_packages_path(python_pkg) == expected);
            }
        }

        // Regression: https://github.com/mamba-org/mamba/issues/4267
        // `micromamba create ... "python=3.14" pip python-freethreading` — noarch :python (pip)
        // must target a directory on sys.path; Windows uses Lib/site-packages for free-threaded
        // CPython, not lib/python3.14t/site-packages.
        TEST_CASE("effective_python_site_packages_path python 3.14 freethreading")
        {
            auto python_pkg = specs::PackageInfo("python");
            python_pkg.version = "3.14.0";
            python_pkg.build_string = "habcdef_0_cp314t";

            SECTION("rewrites std site-packages from repodata for free-threaded 3.14")
            {
                python_pkg.python_site_packages_path =
#ifdef _WIN32
                    "Lib/site-packages";
#else
                    "lib/python3.14/site-packages";
#endif

#ifdef _WIN32
                const auto expected = std::string("Lib/site-packages");
#else
                const auto expected = std::string("lib/python3.14t/site-packages");
#endif
                REQUIRE(effective_python_site_packages_path(python_pkg) == expected);
            }

#ifdef _WIN32
            SECTION("normalizes unix-style free-threaded path on Windows")
            {
                python_pkg.python_site_packages_path = "lib/python3.14t/site-packages";

                REQUIRE(effective_python_site_packages_path(python_pkg) == "Lib/site-packages");
            }
#endif

            SECTION("infers site-packages when repodata omits python_site_packages_path")
            {
                python_pkg.python_site_packages_path.clear();

#ifdef _WIN32
                const auto expected = std::string("Lib/site-packages");
#else
                const auto expected = std::string("lib/python3.14t/site-packages");
#endif
                REQUIRE(effective_python_site_packages_path(python_pkg) == expected);
            }
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

        TEST_CASE("unlink noarch metadata paths")
        {
            const auto tmp_dir = TemporaryDirectory();
            const fs::u8path prefix = tmp_dir.path() / "prefix";
            const fs::u8path conda_meta_dir = prefix / "conda-meta";
            fs::create_directories(conda_meta_dir);

            const fs::u8path site_packages = prefix / "lib" / "python3.14" / "site-packages";
            fs::create_directories(site_packages / "httpx");
            fs::create_directories(site_packages / "httpx-0.27.2.dist-info");

            {
                auto out = open_ofstream(site_packages / "httpx" / "__init__.py");
                out << "print('old')\n";
            }
            {
                auto out = open_ofstream(site_packages / "httpx-0.27.2.dist-info" / "METADATA");
                out << "metadata\n";
            }

            const auto mk_tx_context = [&]
            {
                TransactionParams tx_params{
                    .is_mamba_exe = false,
                    .json_output = false,
                    .verbosity = 0,
                    .shortcuts = false,
                    .envs_dirs = {},
                    .platform = "linux-64",
                    .prefix_params =
                        PrefixParams{
                            .target_prefix = prefix,
                            .root_prefix = prefix,
                            .conda_prefix = prefix,
                            .relocate_prefix = prefix,
                        },
                    .link_params = {},
                    .threads_params = {},
                };

                return TransactionContext(
                    tx_params,
                    { "3.14.4", "3.14.4" },
                    "lib/python3.14/site-packages",
                    {}
                );
            };

            specs::PackageInfo pkg("httpx");
            pkg.version = "0.27.2";
            pkg.build_string = "pyhd8ed1ab_0";

            SECTION("paths_data with short noarch paths is resolved")
            {
                {
                    auto out = open_ofstream(conda_meta_dir / "httpx-0.27.2-pyhd8ed1ab_0.json");
                    out << R"({
  "name": "httpx",
  "version": "0.27.2",
  "build_string": "pyhd8ed1ab_0",
  "paths_data": {
    "paths": [
      { "_path": "site-packages/httpx/__init__.py" },
      { "_path": "site-packages/httpx-0.27.2.dist-info/METADATA" }
    ],
    "paths_version": 1
  }
})";
                }

                auto tx_context = mk_tx_context();
                UnlinkPackage unlink_pkg(pkg, prefix, &tx_context);
                REQUIRE(unlink_pkg.execute());
                REQUIRE_FALSE(fs::exists(site_packages / "httpx" / "__init__.py"));
                REQUIRE_FALSE(fs::exists(site_packages / "httpx-0.27.2.dist-info" / "METADATA"));
            }

            SECTION("legacy files list with noarch short paths is resolved")
            {
                {
                    auto out = open_ofstream(conda_meta_dir / "httpx-0.27.2-pyhd8ed1ab_0.json");
                    out << R"({
  "name": "httpx",
  "version": "0.27.2",
  "build_string": "pyhd8ed1ab_0",
  "files": [
    "site-packages/httpx/__init__.py",
    "site-packages/httpx-0.27.2.dist-info/METADATA"
  ]
})";
                }

                auto tx_context = mk_tx_context();
                UnlinkPackage unlink_pkg(pkg, prefix, &tx_context);
                REQUIRE(unlink_pkg.execute());
                REQUIRE_FALSE(fs::exists(site_packages / "httpx" / "__init__.py"));
                REQUIRE_FALSE(fs::exists(site_packages / "httpx-0.27.2.dist-info" / "METADATA"));
            }
        }
    }
}  // namespace mamba
