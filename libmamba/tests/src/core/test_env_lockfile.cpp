// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>
#include <yaml-cpp/exceptions.h>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/env_lockfile.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/package_database_loader.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/util/algorithm.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace
    {
        auto
        test_absent_file_fails(const fs::u8path file_path, lockfile_parsing_error_code expected_error)
        {
            if (fs::exists(file_path))
            {
                FAIL("file path must not exist" + file_path.string());
                throw "failed";
            }

            const auto maybe_lockfile = read_environment_lockfile(file_path);
            REQUIRE_FALSE(maybe_lockfile);
            const auto error = maybe_lockfile.error();
            REQUIRE(mamba_error_code::env_lockfile_parsing_failed == error.error_code());

            const auto& error_details = EnvLockFileError::get_details(error);
            REQUIRE(expected_error == error_details.parsing_error_code);

            return maybe_lockfile;
        }

        TEST_CASE("env-lockfile absent_file_fails-unknown")
        {
            auto result = test_absent_file_fails(
                "this/file/does/not/exists",
                lockfile_parsing_error_code::not_env_lockfile
            );
            REQUIRE_FALSE(result);
        }

        TEST_CASE("env-lockfile absent_file_fails-conda")
        {
            auto result = test_absent_file_fails(
                "this/file/does/not/exists-lock.yaml",
                lockfile_parsing_error_code::parsing_failure
            );

            REQUIRE_FALSE(result);

            const auto& error_details = EnvLockFileError::get_details(result.error());
            REQUIRE(error_details.error_type);
            const std::type_index bad_file_error_id{ typeid(YAML::BadFile) };
            REQUIRE(bad_file_error_id == error_details.error_type.value());


            // NOTE: one could attempt to check if opening a file which is not an YAML file
            //       would fail. Unfortunately YAML parsers will accept any kind of file,
            //       and assume it is YAML or at worse a comment or raw string. So there
            //       is no good way to check that.
        }

        TEST_CASE("env-lockfile absent_file_fails-mambajs")
        {
            const auto maybe_lockfile = test_absent_file_fails(
                "this/file/does/not/exists.json",
                lockfile_parsing_error_code ::parsing_failure
            );
        }

        auto test_invalid_version_fails(const fs::u8path invalid_version_lockfile_path) -> void
        {
            const auto maybe_lockfile = read_environment_lockfile(invalid_version_lockfile_path);
            REQUIRE_FALSE(maybe_lockfile);
            const auto error = maybe_lockfile.error();
            REQUIRE(mamba_error_code::env_lockfile_parsing_failed == error.error_code());
            const auto& error_details = EnvLockFileError::get_details(error);
            REQUIRE(
                lockfile_parsing_error_code::unsupported_version == error_details.parsing_error_code
            );
        }

        TEST_CASE("env-lockfile invalid_version_fails-conda")
        {
            test_invalid_version_fails(
                mambatests::test_data_dir / "env_lockfile/bad_version-lock.yaml"
            );
        }

        TEST_CASE("env-lockfile invalid_version_fails-mambajs")
        {
            test_invalid_version_fails(
                mambatests::test_data_dir / "env_lockfile/bad_version-lock.json"
            );
        }

        auto test_valid_no_package_succeed(const fs::u8path lockfile_path) -> void
        {
            const auto maybe_lockfile = read_environment_lockfile(lockfile_path);
            if (!maybe_lockfile)
            {
                INFO(maybe_lockfile.error().what());
                FAIL();
                return;
            }
            const auto lockfile = maybe_lockfile.value();
            REQUIRE(lockfile.get_all_packages().empty());
        }

        TEST_CASE("env-lockfile valid_no_package_succeed-conda")
        {
            test_valid_no_package_succeed(
                mambatests::test_data_dir / "env_lockfile/good_no_package-lock.yaml"
            );
        }

        TEST_CASE("env-lockfile valid_no_package_succeed-mambajs")
        {
            test_valid_no_package_succeed(
                mambatests::test_data_dir / "env_lockfile/good_no_package-lock.json"
            );
        }

        auto test_invalid_package_fails(const fs::u8path lockfile_path) -> void
        {
            const auto maybe_lockfile = read_environment_lockfile(lockfile_path);
            REQUIRE_FALSE(maybe_lockfile);
            const auto error = maybe_lockfile.error();
            REQUIRE(mamba_error_code::env_lockfile_parsing_failed == error.error_code());
            const auto& error_details = EnvLockFileError::get_details(error);
            REQUIRE(lockfile_parsing_error_code::parsing_failure == error_details.parsing_error_code);
        }

        TEST_CASE("env-lockfile invalid_package_fails-conda")
        {
            test_invalid_package_fails(
                mambatests::test_data_dir / "env_lockfile/bad_package-lock.yaml"
            );
        }

        TEST_CASE("env-lockfile invalid_package_fails-mambajs")
        {
            test_invalid_package_fails(
                mambatests::test_data_dir / "env_lockfile/bad_package-lock.json"
            );
        }

        auto test_valid_one_package_succeed(const fs::u8path lockfile_path) -> void
        {
            const auto maybe_lockfile = read_environment_lockfile(lockfile_path);
            if (!maybe_lockfile)
            {
                INFO(maybe_lockfile.error().what());
                FAIL();
                return;
            }
            const auto lockfile = maybe_lockfile.value();
            REQUIRE(lockfile.get_all_packages().size() == 1);
        }

        TEST_CASE("env-lockfile valid_one_package_succeed-conda")
        {
            test_valid_one_package_succeed(
                mambatests::test_data_dir / "env_lockfile/good_one_package-lock.yaml"
            );
        }

        TEST_CASE("env-lockfile valid_one_package_succeed-mambajs")
        {
            test_valid_one_package_succeed(
                mambatests::test_data_dir / "env_lockfile/good_one_package-lock.json"
            );
        }

        auto test_valid_one_package_implicit_category(const fs::u8path lockfile_path) -> void
        {
            const auto maybe_lockfile = read_environment_lockfile(lockfile_path);
            if (!maybe_lockfile)
            {
                INFO(maybe_lockfile.error().what());
                FAIL();
                return;
            }
            const auto lockfile = maybe_lockfile.value();
            REQUIRE(lockfile.get_all_packages().size() == 1);
        }

        TEST_CASE("env-lockfile valid_one_package_implicit_category-conda")
        {
            test_valid_one_package_implicit_category(
                mambatests::test_data_dir / "env_lockfile/good_one_package_missing_category-lock.yaml"
            );
        }

        TEST_CASE("env-lockfile valid_one_package_implicit_category-mambajs")
        {
            // NOTE: at the moment of writing this test,
            // categories are not yet part of the mambajs env-lockfile specs
            test_valid_one_package_implicit_category(
                mambatests::test_data_dir / "env_lockfile/good_one_package_missing_category-lock.json"
            );
        }

        auto test_valid_multiple_packages_succeed(const fs::u8path lockfile_path) -> void
        {
            const auto maybe_lockfile = read_environment_lockfile(lockfile_path);
            if (!maybe_lockfile)
            {
                INFO(maybe_lockfile.error().what());
                FAIL();
                return;
            }
            const auto lockfile = maybe_lockfile.value();
            REQUIRE(lockfile.get_all_packages().size() > 1);
        }

        TEST_CASE("env-lockfile valid_multiple_packages_succeed-conda")
        {
            test_valid_multiple_packages_succeed(
                mambatests::test_data_dir / "env_lockfile/good_multiple_packages-lock.yaml"
            );
        }

        TEST_CASE("env-lockfile valid_multiple_packages_succeed-mambajs")
        {
            test_valid_multiple_packages_succeed(
                mambatests::test_data_dir / "env_lockfile/good_multiple_packages-lock.json"
            );
        }

        struct SpecificPackagesRequest
        {
            size_t expected_package_count = 0;
            EnvironmentLockFile::PackageFilter package_filter;
        };

        struct ChannelInfo
        {
            std::string name = {};
            std::vector<std::string> urls = {};
        };

        auto test_get_specific_packages(
            const fs::u8path lockfile_path,
            size_t expected_total_package_count,
            std::vector<SpecificPackagesRequest> requests,
            std::vector<ChannelInfo> expected_channels = {}
        ) -> void
        {
            auto maybe_lockfile = read_environment_lockfile(lockfile_path);
            if (not maybe_lockfile)
            {
                INFO(maybe_lockfile.error().what());
                FAIL();
                return;
            }
            const auto lockfile = maybe_lockfile.value();
            REQUIRE(lockfile.get_packages_for({}).size() == expected_total_package_count);
            REQUIRE(lockfile.get_packages_for({ "", "", "" }).empty());

            for (const auto& request : requests)
            {
                const auto packages = lockfile.get_packages_for(request.package_filter);
                REQUIRE(packages.size() == request.expected_package_count);
            }

            const auto& channels = lockfile.get_metadata().channels;
            for (const auto& expected_channel : expected_channels)
            {
                auto it = std::ranges::find(
                    channels,
                    expected_channel.name,
                    &mamba::EnvironmentLockFile::Channel::name
                );
                REQUIRE(it != channels.end());

                const auto& channel_info = *it;
                for (const auto& url_mirror : expected_channel.urls)
                {
                    REQUIRE(mamba::stdext::contains(channel_info.urls, url_mirror));
                }
            }
        }

        TEST_CASE("env-lockfile get_specific_packages-conda")
        {
            test_get_specific_packages(
                mambatests::test_data_dir / "env_lockfile/good_multiple_packages-lock.yaml",
                15,
                { { 6, { .category = "main", .platform = "linux-64", .manager = "conda" } },
                  { 2, { .category = "main", .platform = "linux-64", .manager = "pip" } } }
            );
        }

        TEST_CASE("env-lockfile get_specific_packages-mambajs")
        {
            test_get_specific_packages(
                mambatests::test_data_dir / "env_lockfile/good_multiple_packages-lock.json",
                51,
                { { 41, { .category = "main", .platform = "emscripten-wasm32", .manager = "conda" } },
                  { 26, { .category = "main", .platform = "noarch", .manager = "conda" } },
                  { 10, { .category = "main", .platform = std::nullopt, .manager = "pip" } } },
                { { .name = "emscripten-forge",
                    .urls = { "https://prefix.dev/emscripten-forge-dev",
                              "https://repo.prefix.dev/emscripten-forge-dev" } },
                  { .name = "conda-forge",
                    .urls = { "https://prefix.dev/conda-forge",
                              "https://repo.prefix.dev/conda-forge" } } }
            );
        }

        TEST_CASE("env-lockfile create_transaction_with_categories")
        {
            // NOTE: at the moment of writing this test,
            // categories are not yet part of the mambajs env-lockfile specs
            // so we only have this test for yaml/conda env-lock-files.

            auto& ctx = mambatests::context();
            const fs::u8path lockfile_path{ mambatests::test_data_dir
                                            / "env_lockfile/good_multiple_categories-lock.yaml" };
            auto channel_context = ChannelContext::make_conda_compatible(mambatests::context());
            solver::libsolv::Database db{ channel_context.params() };
            add_logger_to_database(db);
            mamba::MultiPackageCache pkg_cache({ "/tmp/" }, ctx.validation_params);

            ctx.platform = "linux-64";

            auto check_categories =
                [&](std::vector<std::string> categories, size_t num_conda, size_t num_pip)
            {
                std::vector<detail::other_pkg_mgr_spec> other_specs;
                auto transaction = create_explicit_transaction_from_lockfile(
                    ctx,
                    channel_context,
                    db,
                    lockfile_path,
                    categories,
                    pkg_cache,
                    other_specs
                );
                auto to_install = std::get<1>(transaction.to_conda());
                REQUIRE(to_install.size() == num_conda);
                if (num_pip == 0)
                {
                    REQUIRE(other_specs.size() == 0);
                }
                else
                {
                    REQUIRE(other_specs.size() == 1);
                    REQUIRE(other_specs.at(0).deps.size() == num_pip);
                }
            };

            check_categories({ "main" }, 3, 5);
            check_categories({ "main", "dev" }, 31, 6);
            check_categories({ "dev" }, 28, 1);
            check_categories({ "nonesuch" }, 0, 0);

            ctx.platform = ctx.host_platform;
        }

    }

    namespace
    {
        TEST_CASE("env-lockfile is_conda_env_lockfile_name")
        {
            REQUIRE(is_conda_env_lockfile_name("something-lock.yaml"));
            REQUIRE(is_conda_env_lockfile_name("something-lock.yml"));
            REQUIRE(is_conda_env_lockfile_name("/some/dir/something-lock.yaml"));
            REQUIRE(is_conda_env_lockfile_name("/some/dir/something-lock.yml"));
            REQUIRE(is_conda_env_lockfile_name("../../some/dir/something-lock.yaml"));
            REQUIRE(is_conda_env_lockfile_name("../../some/dir/something-lock.yml"));

            REQUIRE(is_conda_env_lockfile_name(fs::u8path{ "something-lock.yaml" }.string()));
            REQUIRE(is_conda_env_lockfile_name(fs::u8path{ "something-lock.yml" }.string()));
            REQUIRE(is_conda_env_lockfile_name(fs::u8path{ "/some/dir/something-lock.yaml" }.string()));
            REQUIRE(is_conda_env_lockfile_name(fs::u8path{ "/some/dir/something-lock.yml" }.string()));
            REQUIRE(is_conda_env_lockfile_name(
                fs::u8path{ "../../some/dir/something-lock.yaml" }.string()
            ));
            REQUIRE(
                is_conda_env_lockfile_name(fs::u8path{ "../../some/dir/something-lock.yml" }.string())
            );

            REQUIRE_FALSE(is_conda_env_lockfile_name("something"));
            REQUIRE_FALSE(is_conda_env_lockfile_name("something-lock"));
            REQUIRE_FALSE(is_conda_env_lockfile_name("/some/dir/something"));
            REQUIRE_FALSE(is_conda_env_lockfile_name("../../some/dir/something"));

            REQUIRE_FALSE(is_conda_env_lockfile_name("something.yaml"));
            REQUIRE_FALSE(is_conda_env_lockfile_name("something.yml"));
            REQUIRE_FALSE(is_conda_env_lockfile_name("/some/dir/something.yaml"));
            REQUIRE_FALSE(is_conda_env_lockfile_name("/some/dir/something.yml"));
            REQUIRE_FALSE(is_conda_env_lockfile_name("../../some/dir/something.yaml"));
            REQUIRE_FALSE(is_conda_env_lockfile_name("../../some/dir/something.yml"));

            REQUIRE_FALSE(is_conda_env_lockfile_name(fs::u8path{ "something" }.string()));
            REQUIRE_FALSE(is_conda_env_lockfile_name(fs::u8path{ "something-lock" }.string()));
            REQUIRE_FALSE(is_conda_env_lockfile_name(fs::u8path{ "/some/dir/something" }.string()));
            REQUIRE_FALSE(
                is_conda_env_lockfile_name(fs::u8path{ "../../some/dir/something" }.string())
            );

            REQUIRE_FALSE(is_conda_env_lockfile_name("something.json"));
            REQUIRE_FALSE(is_conda_env_lockfile_name("../something.json"));
        }

        TEST_CASE("env-lockfile is_env_lockfile_name")
        {
            REQUIRE(is_env_lockfile_name("something.json"));
            REQUIRE(is_env_lockfile_name("../something.json"));

            REQUIRE(is_env_lockfile_name("something-lock.yaml"));
            REQUIRE(is_env_lockfile_name("something-lock.yml"));
            REQUIRE(is_env_lockfile_name("/some/dir/something-lock.yaml"));
            REQUIRE(is_env_lockfile_name("/some/dir/something-lock.yml"));
            REQUIRE(is_env_lockfile_name("../../some/dir/something-lock.yaml"));
            REQUIRE(is_env_lockfile_name("../../some/dir/something-lock.yml"));

            REQUIRE(is_env_lockfile_name(fs::u8path{ "something-lock.yaml" }.string()));
            REQUIRE(is_env_lockfile_name(fs::u8path{ "something-lock.yml" }.string()));
            REQUIRE(is_env_lockfile_name(fs::u8path{ "/some/dir/something-lock.yaml" }.string()));
            REQUIRE(is_env_lockfile_name(fs::u8path{ "/some/dir/something-lock.yml" }.string()));
            REQUIRE(is_env_lockfile_name(fs::u8path{ "../../some/dir/something-lock.yaml" }.string()));
            REQUIRE(is_env_lockfile_name(fs::u8path{ "../../some/dir/something-lock.yml" }.string()));

            REQUIRE_FALSE(is_env_lockfile_name("something"));
            REQUIRE_FALSE(is_env_lockfile_name("something-lock"));
            REQUIRE_FALSE(is_env_lockfile_name("/some/dir/something"));
            REQUIRE_FALSE(is_env_lockfile_name("../../some/dir/something"));

            REQUIRE_FALSE(is_env_lockfile_name("something.yaml"));
            REQUIRE_FALSE(is_env_lockfile_name("something.yml"));
            REQUIRE_FALSE(is_env_lockfile_name("/some/dir/something.yaml"));
            REQUIRE_FALSE(is_env_lockfile_name("/some/dir/something.yml"));
            REQUIRE_FALSE(is_env_lockfile_name("../../some/dir/something.yaml"));
            REQUIRE_FALSE(is_env_lockfile_name("../../some/dir/something.yml"));

            REQUIRE_FALSE(is_env_lockfile_name(fs::u8path{ "something" }.string()));
            REQUIRE_FALSE(is_env_lockfile_name(fs::u8path{ "something-lock" }.string()));
            REQUIRE_FALSE(is_env_lockfile_name(fs::u8path{ "/some/dir/something" }.string()));
            REQUIRE_FALSE(is_env_lockfile_name(fs::u8path{ "../../some/dir/something" }.string()));
        }

        TEST_CASE("env-lockfile deduce_env_lockfile_format")
        {
            REQUIRE(deduce_env_lockfile_format("something-lock.yaml") == EnvLockfileFormat::conda_yaml);
            REQUIRE(deduce_env_lockfile_format("something-lock.yml") == EnvLockfileFormat::conda_yaml);
            REQUIRE(
                deduce_env_lockfile_format("/some/dir/something-lock.yaml")
                == EnvLockfileFormat::conda_yaml
            );
            REQUIRE(
                deduce_env_lockfile_format("/some/dir/something-lock.yml")
                == EnvLockfileFormat::conda_yaml
            );
            REQUIRE(
                deduce_env_lockfile_format("../../some/dir/something-lock.yaml")
                == EnvLockfileFormat::conda_yaml
            );
            REQUIRE(
                deduce_env_lockfile_format("../../some/dir/something-lock.yml")
                == EnvLockfileFormat::conda_yaml
            );

            REQUIRE(deduce_env_lockfile_format("something") == EnvLockfileFormat::undefined);
            REQUIRE(deduce_env_lockfile_format("something-lock") == EnvLockfileFormat::undefined);
            REQUIRE(deduce_env_lockfile_format("/some/dir/something") == EnvLockfileFormat::undefined);
            REQUIRE(
                deduce_env_lockfile_format("../../some/dir/something") == EnvLockfileFormat::undefined
            );

            REQUIRE(deduce_env_lockfile_format("something.yaml") == EnvLockfileFormat::undefined);
            REQUIRE(deduce_env_lockfile_format("something.yml") == EnvLockfileFormat::undefined);
            REQUIRE(
                deduce_env_lockfile_format("/some/dir/something.yaml") == EnvLockfileFormat::undefined
            );
            REQUIRE(
                deduce_env_lockfile_format("/some/dir/something.yml") == EnvLockfileFormat::undefined
            );
            REQUIRE(
                deduce_env_lockfile_format("../../some/dir/something.yaml")
                == EnvLockfileFormat::undefined
            );
            REQUIRE(
                deduce_env_lockfile_format("../../some/dir/something.yml") == EnvLockfileFormat::undefined
            );

            REQUIRE(deduce_env_lockfile_format("something.json") == EnvLockfileFormat::mambajs_json);
            REQUIRE(
                deduce_env_lockfile_format("truc.something.json") == EnvLockfileFormat::mambajs_json
            );
            REQUIRE(
                deduce_env_lockfile_format("../machin/something.json") == EnvLockfileFormat::mambajs_json
            );
            REQUIRE(
                deduce_env_lockfile_format("../machin/truc.something.json")
                == EnvLockfileFormat::mambajs_json
            );
        }
    }

}
