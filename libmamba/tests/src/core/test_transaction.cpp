// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <stack>
#include <string>

#include <catch2/catch_all.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/solution.hpp"
#include "mamba/specs/archive.hpp"
#include "mamba/validation/tools.hpp"

#include "core/link.hpp"
#include "core/transaction_context.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace
    {
        struct ScopedContextOverride
        {
            ScopedContextOverride(Context& ctx, const fs::u8path& prefix, const fs::u8path& pkgs_dir)
            {
                m_ctx = &ctx;
                m_saved_prefix_params = ctx.prefix_params;
                m_saved_pkgs_dirs = ctx.pkgs_dirs;
                m_saved_prefix_data_interoperability = ctx.prefix_data_interoperability;
                m_saved_extract_threads = ctx.threads_params.extract_threads;

                ctx.prefix_params.target_prefix = prefix;
                ctx.prefix_params.root_prefix = prefix;
                ctx.prefix_params.conda_prefix = prefix;
                ctx.prefix_params.relocate_prefix = prefix;
                ctx.pkgs_dirs = { pkgs_dir };
                ctx.prefix_data_interoperability = false;
                ctx.threads_params.extract_threads = 1;
            }

            ~ScopedContextOverride()
            {
                m_ctx->prefix_params = m_saved_prefix_params;
                m_ctx->pkgs_dirs = m_saved_pkgs_dirs;
                m_ctx->prefix_data_interoperability = m_saved_prefix_data_interoperability;
                m_ctx->threads_params.extract_threads = m_saved_extract_threads;
            }

            Context* m_ctx = nullptr;
            PrefixParams m_saved_prefix_params;
            std::vector<fs::u8path> m_saved_pkgs_dirs;
            bool m_saved_prefix_data_interoperability = false;
            int m_saved_extract_threads = 0;
        };

        void write_text_file(const fs::u8path& path, std::string_view content)
        {
            fs::create_directories(path.parent_path());
            std::ofstream out(path.std_path());
            out << content;
        }

        specs::PackageInfo make_test_package(const std::string& name)
        {
            specs::PackageInfo pkg(name);
            pkg.version = "1.0.0";
            pkg.build_string = "h0_0";
            pkg.build_number = 0;
            pkg.platform = "noarch";
            pkg.channel = "https://conda.anaconda.org/conda-forge/noarch";
            pkg.filename = fmt::format("{}-{}-{}.tar.bz2", name, pkg.version, pkg.build_string);
            pkg.package_url = pkg.channel + "/" + pkg.filename;
            return pkg;
        }

        void write_repodata_record(const fs::u8path& extract_dir, const specs::PackageInfo& pkg)
        {
            nlohmann::json repodata{
                { "name", pkg.name },          { "version", pkg.version },
                { "build", pkg.build_string }, { "build_number", pkg.build_number },
                { "url", pkg.package_url },    { "channel", pkg.channel },
                { "size", pkg.size },
            };
            if (!pkg.md5.empty())
            {
                repodata["md5"] = pkg.md5;
            }
            if (!pkg.sha256.empty())
            {
                repodata["sha256"] = pkg.sha256;
            }
            write_text_file(extract_dir / "info" / "repodata_record.json", repodata.dump());
        }

        void create_extracted_package(
            const fs::u8path& pkgs_dir,
            const specs::PackageInfo& pkg,
            const std::string& rel_file,
            std::string_view content,
            bool include_file = true
        )
        {
            const std::string pkg_dir_name = std::string(specs::strip_archive_extension(pkg.filename));
            const fs::u8path extract_dir = pkgs_dir / pkg_dir_name;
            const fs::u8path file_path = extract_dir / rel_file;

            if (include_file)
            {
                write_text_file(file_path, content);
            }

            nlohmann::json paths_json;
            paths_json["paths_version"] = 1;
            if (include_file)
            {
                paths_json["paths"] = nlohmann::json::array(
                    {
                        nlohmann::json{
                            { "_path", rel_file },
                            { "path_type", "hardlink" },
                            { "sha256", validation::sha256sum(file_path) },
                            { "size_in_bytes", fs::file_size(file_path) },
                        },
                    }
                );
            }
            else
            {
                paths_json["paths"] = nlohmann::json::array(
                    {
                        nlohmann::json{
                            { "_path", rel_file },
                            { "path_type", "hardlink" },
                            { "sha256", std::string(64, '0') },
                            { "size_in_bytes", 42 },
                        },
                    }
                );
            }

            nlohmann::json index_json{
                { "name", pkg.name },
                { "version", pkg.version },
                { "build", pkg.build_string },
                { "build_number", pkg.build_number },
            };

            write_text_file(extract_dir / "info" / "paths.json", paths_json.dump());
            write_text_file(extract_dir / "info" / "index.json", index_json.dump());
            write_repodata_record(extract_dir, pkg);
        }

        TransactionContext make_transaction_context(const fs::u8path& prefix)
        {
            TransactionParams tx_params{
                .is_mamba_exe = false,
                .json_output = false,
                .verbosity = 0,
                .shortcuts = false,
                .envs_dirs = {},
                .platform = "noarch",
                .prefix_params =
                    PrefixParams{
                        .target_prefix = prefix,
                        .root_prefix = prefix,
                        .conda_prefix = prefix,
                        .relocate_prefix = prefix,
                    },
                .link_params = { .compile_pyc = false },
                .threads_params = {},
            };
            return TransactionContext(tx_params, { "", "" }, "", {});
        }

        void link_package_to_prefix(
            const specs::PackageInfo& pkg,
            const fs::u8path& pkgs_dir,
            const fs::u8path& prefix
        )
        {
            fs::create_directories(prefix / "conda-meta");
            auto tx_context = make_transaction_context(prefix);
            LinkPackage linker(pkg, pkgs_dir, &tx_context);
            REQUIRE(linker.execute());
        }

        bool prefix_has_package_file(
            const fs::u8path& prefix,
            const specs::PackageInfo& pkg,
            const std::string& rel_file
        )
        {
            return fs::exists(prefix / rel_file)
                   && fs::exists(prefix / "conda-meta" / (pkg.str() + ".json"));
        }

        void rollback_recorded_link_and_unlink_operations(
            std::stack<LinkPackage>& linked_packages,
            std::stack<UnlinkPackage>& unlinked_packages
        )
        {
            while (!linked_packages.empty())
            {
                linked_packages.top().undo();
                linked_packages.pop();
            }

            while (!unlinked_packages.empty())
            {
                unlinked_packages.top().undo();
                unlinked_packages.pop();
            }
        }
    }

    // Non-regression for https://github.com/mamba-org/mamba/issues/4322
    // Invalid extracted caches (missing files listed in paths.json) must not be used for linking.
    TEST_CASE(
        "Invalid extracted cache is rejected when paths.json file is missing #4322",
        "[mamba::core][transaction][regression][4322]"
    )
    {
        auto& ctx = mambatests::context();
        TemporaryDirectory temp_dir;
        const fs::u8path pkgs_dir = temp_dir.path() / "pkgs";
        fs::create_directories(pkgs_dir);

        auto pkg = make_test_package("pkg-cache-a");
        const std::string rel_file = "share/pkg-cache-a/hello.txt";
        create_extracted_package(pkgs_dir, pkg, rel_file, "hello\n", /* include_file= */ false);

        MultiPackageCache cache({ pkgs_dir }, ctx.validation_params);
        REQUIRE(cache.get_extracted_dir_path(pkg).empty());
    }

    // Non-regression for https://github.com/mamba-org/mamba/issues/4322
    // After extracting from a tarball, stale negative cache entries must not block linking.
    TEST_CASE(
        "Stale negative cache recovers after tarball extraction #4322",
        "[mamba::core][transaction][regression][4322]"
    )
    {
        auto& ctx = mambatests::context();
        TemporaryDirectory temp_dir;
        const fs::u8path pkgs_dir = temp_dir.path() / "pkgs";
        fs::create_directories(pkgs_dir);

        auto pkg = make_test_package("pkg-cache-b");
        const std::string rel_file = "share/pkg-cache-b/hello.txt";
        const std::string pkg_dir_name = std::string(specs::strip_archive_extension(pkg.filename));
        const fs::u8path extract_dir = pkgs_dir / pkg_dir_name;

        create_extracted_package(pkgs_dir, pkg, rel_file, "hello\n");
        create_archive(
            extract_dir,
            pkgs_dir / pkg.filename,
            compression_algorithm::bzip2,
            /* compression_level= */ 1,
            /* compression_threads= */ 1,
            /* filter= */ nullptr
        );

        // Corrupt the extracted directory while keeping a valid tarball, as in the issue report.
        fs::remove(extract_dir / rel_file);

        MultiPackageCache cache({ pkgs_dir }, ctx.validation_params);
        REQUIRE(cache.get_extracted_dir_path(pkg).empty());

        ExtractOptions options;
        options.sparse = false;
        options.subproc_mode = extract_subproc_mode::mamba_package;
        fs::remove_all(extract_dir);
        mamba::extract(pkgs_dir / pkg.filename, extract_dir, options);
        write_repodata_record(extract_dir, pkg);
        cache.clear_query_cache(pkg);

        REQUIRE(cache.get_extracted_dir_path(pkg) == pkgs_dir);
        REQUIRE(fs::exists(extract_dir / rel_file));
    }

    // Non-regression for https://github.com/mamba-org/mamba/issues/4322
    // A package cache error during linking must roll back earlier unlink/link operations.
    TEST_CASE(
        "Link and unlink rollback restores prefix after package cache error #4322",
        "[mamba::core][transaction][regression][4322]"
    )
    {
        TemporaryDirectory temp_dir;
        const fs::u8path prefix = temp_dir.path() / "prefix";
        const fs::u8path pkgs_dir = temp_dir.path() / "pkgs";
        fs::create_directories(pkgs_dir);

        auto pkg_a = make_test_package("pkg-a");
        auto pkg_b = make_test_package("pkg-b");
        const std::string rel_file = "share/pkg-a/hello.txt";
        create_extracted_package(pkgs_dir, pkg_a, rel_file, "hello from pkg-a\n");
        link_package_to_prefix(pkg_a, pkgs_dir, prefix);
        REQUIRE(prefix_has_package_file(prefix, pkg_a, rel_file));

        auto tx_context = make_transaction_context(prefix);
        std::stack<UnlinkPackage> unlinked_packages;
        std::stack<LinkPackage> linked_packages;

        UnlinkPackage unlink_pkg_a(pkg_a, pkgs_dir, &tx_context);
        REQUIRE(unlink_pkg_a.execute());
        unlinked_packages.push(std::move(unlink_pkg_a));
        REQUIRE_FALSE(fs::exists(prefix / rel_file));

        LinkPackage relink_pkg_a(pkg_a, pkgs_dir, &tx_context);
        REQUIRE(relink_pkg_a.execute());
        linked_packages.push(std::move(relink_pkg_a));
        REQUIRE(prefix_has_package_file(prefix, pkg_a, rel_file));

        LinkPackage link_pkg_b(pkg_b, pkgs_dir, &tx_context);
        REQUIRE_THROWS_AS(link_pkg_b.execute(), std::runtime_error);

        rollback_recorded_link_and_unlink_operations(linked_packages, unlinked_packages);
        REQUIRE(prefix_has_package_file(prefix, pkg_a, rel_file));
        REQUIRE_FALSE(fs::exists(prefix / "conda-meta" / (pkg_b.str() + ".json")));
    }

    // Non-regression for https://github.com/mamba-org/mamba/issues/4322
    // When a package cannot be fetched, the prefix must remain unchanged.
    TEST_CASE(
        "Transaction leaves prefix intact when fetch fails before linking #4322",
        "[mamba::core][transaction][regression][4322]"
    )
    {
        auto& ctx = mambatests::context();
        TemporaryDirectory temp_dir;
        const fs::u8path prefix = temp_dir.path() / "prefix";
        const fs::u8path pkgs_dir = temp_dir.path() / "pkgs";
        fs::create_directories(pkgs_dir);
        ScopedContextOverride context_guard(ctx, prefix, pkgs_dir);

        auto pkg_a = make_test_package("pkg-a");
        auto pkg_b = make_test_package("pkg-b");
        const std::string rel_file = "share/pkg-a/hello.txt";
        create_extracted_package(pkgs_dir, pkg_a, rel_file, "hello from pkg-a\n");
        link_package_to_prefix(pkg_a, pkgs_dir, prefix);
        REQUIRE(prefix_has_package_file(prefix, pkg_a, rel_file));

        auto channel_context = ChannelContext::make_simple(ctx);
        auto db = solver::libsolv::Database(channel_context.params());
        PrefixData prefix_data = PrefixData::create(prefix, channel_context, true).value();
        MultiPackageCache package_caches({ pkgs_dir }, ctx.validation_params);

        solver::Request request;
        solver::Solution solution{ {
            solver::Solution::Remove{ pkg_a },
            solver::Solution::Install{ pkg_a },
            solver::Solution::Install{ pkg_b },
        } };

        MTransaction transaction(ctx, db, request, std::move(solution), package_caches);

        REQUIRE_THROWS_AS(transaction.execute(ctx, channel_context, prefix_data), std::exception);
        REQUIRE(prefix_has_package_file(prefix, pkg_a, rel_file));
        REQUIRE_FALSE(fs::exists(prefix / "conda-meta" / (pkg_b.str() + ".json")));
    }

    // Non-regression for https://github.com/mamba-org/mamba/issues/4322
    TEST_CASE(
        "UnlinkPackage undo restores a previously linked package #4322",
        "[mamba::core][transaction][regression][4322]"
    )
    {
        TemporaryDirectory temp_dir;
        const fs::u8path prefix = temp_dir.path() / "prefix";
        const fs::u8path pkgs_dir = temp_dir.path() / "pkgs";
        fs::create_directories(pkgs_dir);

        auto pkg = make_test_package("pkg-undo");
        const std::string rel_file = "share/pkg-undo/hello.txt";
        create_extracted_package(pkgs_dir, pkg, rel_file, "undo me\n");
        link_package_to_prefix(pkg, pkgs_dir, prefix);
        REQUIRE(prefix_has_package_file(prefix, pkg, rel_file));

        auto tx_context = make_transaction_context(prefix);
        UnlinkPackage unlinker(pkg, pkgs_dir, &tx_context);
        REQUIRE(unlinker.execute());
        REQUIRE_FALSE(fs::exists(prefix / rel_file));

        REQUIRE(unlinker.undo());
        REQUIRE(prefix_has_package_file(prefix, pkg, rel_file));
    }
}  // namespace mamba
