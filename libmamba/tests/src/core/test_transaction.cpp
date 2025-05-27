// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/package_info.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace
    {
        TEST_CASE("MTransaction constructor with packages")
        {
            auto& ctx = mambatests::context();
            specs::ChannelResolveParams channel_params;
            auto database = solver::libsolv::Database(channel_params);

            std::vector<specs::PackageInfo> pkgs_to_install;
            pkgs_to_install.emplace_back("test-pkg1", "1.0.0", "build1", "conda-forge");

            std::vector<specs::PackageInfo> pkgs_to_remove;
            pkgs_to_remove.emplace_back("test-pkg2", "2.0.0", "build2", "conda-forge");

            std::vector<fs::u8path> pkgs_dirs = { "test_cache" };
            ValidationParams params;
            MultiPackageCache caches(pkgs_dirs, params);

            // Should throw since pkg2 is not in database
            REQUIRE_THROWS_AS(
                MTransaction(ctx, database, pkgs_to_remove, pkgs_to_install, caches),
                std::runtime_error
            );
        }

        TEST_CASE("MTransaction empty and to_conda")
        {
            auto& ctx = mambatests::context();
            specs::ChannelResolveParams channel_params;
            auto database = solver::libsolv::Database(channel_params);
            std::vector<specs::PackageInfo> pkgs;
            std::vector<fs::u8path> pkgs_dirs = { "test_cache" };
            ValidationParams params;
            MultiPackageCache caches(pkgs_dirs, params);

            MTransaction transaction(ctx, database, pkgs, caches);
            REQUIRE(transaction.empty());
            auto conda_tuple = transaction.to_conda();
            // Just check that the tuple can be unpacked
            auto& [specs_tuple, to_install, to_remove] = conda_tuple;
            (void) specs_tuple;
            (void) to_install;
            (void) to_remove;
        }

        TEST_CASE("MTransaction log_json does not throw")
        {
            auto& ctx = mambatests::context();
            specs::ChannelResolveParams channel_params;
            auto database = solver::libsolv::Database(channel_params);
            std::vector<specs::PackageInfo> pkgs;
            std::vector<fs::u8path> pkgs_dirs = { "test_cache" };
            ValidationParams params;
            MultiPackageCache caches(pkgs_dirs, params);

            MTransaction transaction(ctx, database, pkgs, caches);
            REQUIRE_NOTHROW(transaction.log_json());
        }

        TEST_CASE("MTransaction print, prompt, fetch_extract_packages, execute")
        {
            auto& ctx = mambatests::context();
            specs::ChannelResolveParams channel_params;
            auto database = solver::libsolv::Database(channel_params);
            std::vector<specs::PackageInfo> pkgs;
            std::vector<fs::u8path> pkgs_dirs = { "test_cache" };
            ValidationParams params;
            MultiPackageCache caches(pkgs_dirs, params);
            ChannelContext channel_context = ChannelContext::make_conda_compatible(ctx);
            // PrefixData prefix_data("/tmp/test_prefix", channel_context, false); // private
            // constructor

            MTransaction transaction(ctx, database, pkgs, caches);
            // These should not throw for an empty transaction
            REQUIRE_NOTHROW(transaction.print(ctx, channel_context));
            REQUIRE_NOTHROW(transaction.prompt(ctx, channel_context));
            REQUIRE_NOTHROW(transaction.fetch_extract_packages(ctx, channel_context));
            // REQUIRE_NOTHROW(transaction.execute(ctx, channel_context, prefix_data));
        }

        TEST_CASE("MTransaction create from urls and lockfile throws")
        {
            auto& ctx = mambatests::context();
            specs::ChannelResolveParams channel_params;
            auto database = solver::libsolv::Database(channel_params);
            std::vector<fs::u8path> pkgs_dirs = { "test_cache" };
            ValidationParams params;
            MultiPackageCache caches(pkgs_dirs, params);
            std::vector<detail::other_pkg_mgr_spec> other_specs;
            ChannelContext channel_context = ChannelContext::make_conda_compatible(ctx);

            std::vector<std::string> urls = {
                "https://conda.anaconda.org/conda-forge/linux-64/invalid-package.tar.bz2"
            };
            REQUIRE_THROWS_AS(
                create_explicit_transaction_from_urls(ctx, database, urls, caches, other_specs),
                specs::ParseError
            );

            fs::u8path lockfile_path = "test.lock";
            std::vector<std::string> categories = { "main" };
            REQUIRE_THROWS_AS(
                create_explicit_transaction_from_lockfile(
                    ctx,
                    database,
                    lockfile_path,
                    categories,
                    caches,
                    other_specs
                ),
                std::runtime_error
            );
        }

        TEST_CASE("MTransaction constructor with solver request")
        {
            auto& ctx = mambatests::context();
            auto channel_context = ChannelContext::make_conda_compatible(ctx);
            specs::ChannelResolveParams channel_params;
            auto database = solver::libsolv::Database(channel_params);

            solver::Request request;
            request.flags.keep_user_specs = true;

            // Add some specs to the request
            request.jobs.push_back(solver::Request::Install{
                specs::MatchSpec::parse("python=3.8").value() });
            request.jobs.push_back(solver::Request::Remove{
                specs::MatchSpec::parse("old-pkg").value() });

            solver::Solution solution;
            solution.actions.push_back(solver::Solution::Install{
                specs::PackageInfo("python", "3.8.0", "build", "conda-forge") });
            solution.actions.push_back(solver::Solution::Remove{
                specs::PackageInfo("old-pkg", "1.0.0", "build", "conda-forge") });

            std::vector<fs::u8path> pkgs_dirs = { "test_cache" };
            ValidationParams params;
            MultiPackageCache caches(pkgs_dirs, params);

            MTransaction transaction(ctx, database, request, std::move(solution), caches);

            // Check that user specs were kept
            auto [specs_tuple, to_install, to_remove] = transaction.to_conda();
            auto& [update, remove] = specs_tuple;
            REQUIRE(update.size() == 1);
            REQUIRE(remove.size() == 1);
            REQUIRE(update[0] == "python=3.8");
            REQUIRE(remove[0] == "old-pkg");
        }
    }
}  // namespace mamba
