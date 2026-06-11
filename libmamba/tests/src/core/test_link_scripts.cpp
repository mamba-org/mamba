// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>

#include <catch2/catch_all.hpp>

#include "mamba/core/fsutil.hpp"
#include "mamba/core/util.hpp"
#include "mamba/specs/package_info.hpp"

#include "core/link.hpp"
#include "core/transaction_context.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace
    {
        TEST_CASE("run_scripts_in_transaction")
        {
            // Ensure Console is initialized to avoid singleton access errors
            (void) mambatests::context();

            const auto tmp_dir = TemporaryDirectory();
            const fs::u8path prefix = tmp_dir.path() / "prefix";
            const fs::u8path cache_dir = tmp_dir.path() / "cache";

            specs::PackageInfo pkg("test_pkg");
            pkg.version = "1.0";
            pkg.build_string = "0";

            const fs::u8path pkg_source = cache_dir / pkg.str();
            const fs::u8path bin_dir_relative = get_bin_directory_short_path();

            fs::create_directories(prefix / "conda-meta");
            fs::create_directories(pkg_source / bin_dir_relative);
            fs::create_directories(prefix / bin_dir_relative);

            const auto make_tx_context = [&]
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

            std::string script_ext = util::on_win ? ".bat" : ".sh";
            std::string pre_link_name = ".test_pkg-pre-link" + script_ext;
            std::string post_link_name = ".test_pkg-post-link" + script_ext;
            std::string pre_unlink_name = ".test_pkg-pre-unlink" + script_ext;
            std::string post_unlink_name = ".test_pkg-post-unlink" + script_ext;

            auto create_script = [](const fs::u8path& p, const fs::u8path& marker_path) {
                std::ofstream out = open_ofstream(p);
                if (util::on_win)
                {
                    out << "@echo off\n";
                    out << "echo done > \"" << marker_path.string() << "\"\n";
                }
                else
                {
                    out << "#!/bin/sh\n";
                    out << "echo done > \"" << marker_path.string() << "\"\n";
                }
            };

            fs::u8path pre_link_marker = tmp_dir.path() / "pre_link_done.txt";
            fs::u8path post_link_marker = tmp_dir.path() / "post_link_done.txt";
            fs::u8path pre_unlink_marker = tmp_dir.path() / "pre_unlink_done.txt";
            fs::u8path post_unlink_marker = tmp_dir.path() / "post_unlink_done.txt";

            create_script(pkg_source / bin_dir_relative / pre_link_name, pre_link_marker);
            create_script(prefix / bin_dir_relative / post_link_name, post_link_marker);
            create_script(prefix / bin_dir_relative / pre_unlink_name, pre_unlink_marker);
            create_script(prefix / bin_dir_relative / post_unlink_name, post_unlink_marker);

            // Prepare metadata for unlinking
            {
                auto out = open_ofstream(prefix / "conda-meta" / (pkg.str() + ".json"));
                out << R"({
                    "name": "test_pkg",
                    "version": "1.0",
                    "build_string": "0",
                    "paths_data": { "paths": [], "paths_version": 1 }
                    })";
            }
            // Prepare paths.json for linking
            {
                fs::create_directories(pkg_source / "info");
                auto out = open_ofstream(pkg_source / "info" / "paths.json");
                out << R"({ "paths": [], "paths_version": 1 })";
            }
            // Prepare repodata_record.json for linking
            {
                auto out = open_ofstream(pkg_source / "info" / "repodata_record.json");
                out << R"({ "noarch": null })";
            }

            SECTION("linking executes pre-link and post-link")
            {
                auto tx_context = make_tx_context();
                LinkPackage link_pkg(pkg, cache_dir, &tx_context);

                REQUIRE(link_pkg.execute());

                REQUIRE(fs::exists(pre_link_marker));
                REQUIRE(fs::exists(post_link_marker));
            }

            SECTION("unlinking executes pre-unlink and post-unlink")
            {
                auto tx_context = make_tx_context();
                UnlinkPackage unlink_pkg(pkg, cache_dir, &tx_context);

                REQUIRE(unlink_pkg.execute());

                REQUIRE(fs::exists(pre_unlink_marker));
                // post-unlink script should not be executed as deprecated
                REQUIRE(!fs::exists(post_unlink_marker));
            }
        }
    }
}  // namespace mamba
