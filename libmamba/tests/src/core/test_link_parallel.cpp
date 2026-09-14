// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <string>

#include <catch2/catch_all.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "mamba/core/fsutil.hpp"
#include "mamba/core/package_paths.hpp"
#include "mamba/core/util.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/validation/tools.hpp"

#include "core/link.hpp"
#include "core/transaction_context.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace
    {
        auto make_link_params() -> LinkParams
        {
            return {
                .allow_softlinks = false,
                .always_copy = false,
                .always_softlink = false,
                .compile_pyc = false,
                .skip_run_link_scripts = true,
            };
        }

        auto make_tx_context(const fs::u8path& prefix, int link_threads) -> TransactionContext
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
                .link_params = make_link_params(),
                .threads_params =
                    ThreadsParams{
                        .download_threads = 1,
                        .extract_threads = 1,
                        .link_threads = link_threads,
                    },
            };

            return TransactionContext(tx_params, { "", "" }, "", {});
        }

        auto prepare_synthetic_package(const fs::u8path& cache_dir, const specs::PackageInfo& pkg)
            -> fs::u8path
        {
            const fs::u8path pkg_source = cache_dir / pkg.str();
            fs::create_directories(pkg_source / "info");
            fs::create_directories(pkg_source / "lib" / "data");

            nlohmann::json paths = nlohmann::json::object();
            paths["paths_version"] = 1;
            paths["paths"] = nlohmann::json::array();

            // Many hardlink files under nested directories.
            constexpr int n_files = 64;
            for (int i = 0; i < n_files; ++i)
            {
                const auto rel = fmt::format("lib/data/file_{:03d}.txt", i);
                const auto full = pkg_source / rel;
                {
                    auto out = open_ofstream(full);
                    out << "content-" << i << '\n';
                }
                const auto sha = validation::sha256sum(full);
                paths["paths"].push_back(
                    {
                        { "_path", rel },
                        { "path_type", "hardlink" },
                        { "sha256", sha },
                        { "size_in_bytes", fs::file_size(full) },
                    }
                );
            }

            // Softlink pointing at the first hardlink file (relative target).
            {
                const std::string link_rel = "lib/data/file_000_link.txt";
                fs::create_symlink("file_000.txt", pkg_source / link_rel);
                paths["paths"].push_back(
                    {
                        { "_path", link_rel },
                        { "path_type", "softlink" },
                        { "size_in_bytes", 0 },
                    }
                );
            }

            // Prefix-placeholder text file (must be copied + rewritten).
            {
                const std::string placeholder = std::string(PREFIX_PLACEHOLDER_1)
                                                + PREFIX_PLACEHOLDER_2;
                const std::string rel = "lib/data/prefix_file.txt";
                const auto full = pkg_source / rel;
                {
                    auto out = open_ofstream(full);
                    out << "prefix=" << placeholder << "\n";
                }
                paths["paths"].push_back(
                    {
                        { "_path", rel },
                        { "path_type", "hardlink" },
                        { "sha256", validation::sha256sum(full) },
                        { "size_in_bytes", fs::file_size(full) },
                        { "prefix_placeholder", placeholder },
                        { "file_mode", "text" },
                    }
                );
            }

            {
                auto out = open_ofstream(pkg_source / "info" / "paths.json");
                out << paths.dump(4);
            }
            {
                auto out = open_ofstream(pkg_source / "info" / "repodata_record.json");
                out << R"({ "name": "test_parallel_link", "version": "1.0", "build": "0", "noarch": null })";
            }

            return pkg_source;
        }

        auto read_conda_meta(const fs::u8path& prefix, const specs::PackageInfo& pkg) -> nlohmann::json
        {
            std::ifstream in = open_ifstream(prefix / "conda-meta" / (pkg.str() + ".json"));
            nlohmann::json j;
            in >> j;
            return j;
        }

        TEST_CASE("parallel_link_matches_sequential")
        {
            (void) mambatests::context();

            const auto tmp_dir = TemporaryDirectory();
            const fs::u8path cache_dir = tmp_dir.path() / "cache";

            specs::PackageInfo pkg("test_parallel_link");
            pkg.version = "1.0";
            pkg.build_string = "0";

            prepare_synthetic_package(cache_dir, pkg);

            // Same prefix path for both runs so prefix-replaced content (and hashes) match.
            const fs::u8path prefix = tmp_dir.path() / "prefix";

            const auto link_into_fresh_prefix = [&](int link_threads) -> nlohmann::json
            {
                fs::remove_all(prefix);
                fs::create_directories(prefix / "conda-meta");
                auto tx = make_tx_context(prefix, link_threads);
                LinkPackage link_pkg(pkg, cache_dir, &tx);
                REQUIRE(link_pkg.execute());
                return read_conda_meta(prefix, pkg);
            };

            const auto meta_serial = link_into_fresh_prefix(/*link_threads=*/1);
            const auto meta_parallel = link_into_fresh_prefix(/*link_threads=*/4);

            REQUIRE(meta_serial["files"] == meta_parallel["files"]);
            REQUIRE(
                meta_serial["paths_data"]["paths"].size()
                == meta_parallel["paths_data"]["paths"].size()
            );

            for (std::size_t i = 0; i < meta_serial["paths_data"]["paths"].size(); ++i)
            {
                const auto& a = meta_serial["paths_data"]["paths"][i];
                const auto& b = meta_parallel["paths_data"]["paths"][i];
                REQUIRE(a["_path"] == b["_path"]);
                REQUIRE(a["sha256_in_prefix"] == b["sha256_in_prefix"]);
                if (a.contains("path_type"))
                {
                    REQUIRE(a["path_type"] == b["path_type"]);
                }
            }

            // Prefix placeholder was rewritten to the target prefix.
            const auto rewritten = read_contents(prefix / "lib" / "data" / "prefix_file.txt");
            REQUIRE(rewritten.find(prefix.string()) != std::string::npos);
            REQUIRE(
                rewritten.find(std::string(PREFIX_PLACEHOLDER_1) + PREFIX_PLACEHOLDER_2)
                == std::string::npos
            );

            // Softlink exists and points at the hardlinked content.
            REQUIRE(fs::is_symlink(prefix / "lib" / "data" / "file_000_link.txt"));
            REQUIRE(fs::exists(prefix / "lib" / "data" / "file_000.txt"));
        }
    }
}  // namespace mamba
