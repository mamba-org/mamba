// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>
#include <yaml-cpp/exceptions.h>

#include "mamba/core/channel.hpp"
#include "mamba/core/env_lockfile.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/transaction.hpp"

#include "test_data.hpp"

namespace mamba
{
    TEST_SUITE("env_lockfile")
    {
        TEST_CASE("absent_file_fails")
        {
            ChannelContext channel_context{ Context::instance() };
            const auto maybe_lockfile = read_environment_lockfile(
                channel_context,
                "this/file/does/not/exists"
            );
            REQUIRE_FALSE(maybe_lockfile);
            const auto error = maybe_lockfile.error();
            REQUIRE_EQ(mamba_error_code::env_lockfile_parsing_failed, error.error_code());

            const auto& error_details = EnvLockFileError::get_details(error);
            CHECK_EQ(file_parsing_error_code::parsing_failure, error_details.parsing_error_code);
            REQUIRE(error_details.yaml_error_type);
            const std::type_index bad_file_error_id{ typeid(YAML::BadFile) };
            CHECK_EQ(bad_file_error_id, error_details.yaml_error_type.value());

            // NOTE: one could attempt to check if opening a file which is not an YAML file
            //       would fail. Unfortunately YAML parsers will accept any kind of file,
            //       and assume it is YAML or at worse a comment or raw string. So there
            //       is no good way to check that.
        }

        TEST_CASE("invalid_version_fails")
        {
            ChannelContext channel_context{ Context::instance() };
            const fs::u8path invalid_version_lockfile_path{ test_data_dir
                                                            / "env_lockfile/bad_version-lock.yaml" };
            const auto maybe_lockfile = read_environment_lockfile(
                channel_context,
                invalid_version_lockfile_path
            );
            REQUIRE_FALSE(maybe_lockfile);
            const auto error = maybe_lockfile.error();
            REQUIRE_EQ(mamba_error_code::env_lockfile_parsing_failed, error.error_code());
            const auto& error_details = EnvLockFileError::get_details(error);
            CHECK_EQ(file_parsing_error_code::unsuported_version, error_details.parsing_error_code);
        }

        TEST_CASE("valid_no_package_succeed")
        {
            ChannelContext channel_context{ Context::instance() };
            const fs::u8path lockfile_path{ test_data_dir / "env_lockfile/good_no_package-lock.yaml" };
            const auto maybe_lockfile = read_environment_lockfile(channel_context, lockfile_path);
            REQUIRE_MESSAGE(maybe_lockfile, maybe_lockfile.error().what());
            const auto lockfile = maybe_lockfile.value();
            CHECK(lockfile.get_all_packages().empty());
        }

        TEST_CASE("invalid_package_fails")
        {
            ChannelContext channel_context{ Context::instance() };
            const fs::u8path lockfile_path{ test_data_dir / "env_lockfile/bad_package-lock.yaml" };
            const auto maybe_lockfile = read_environment_lockfile(channel_context, lockfile_path);
            REQUIRE_FALSE(maybe_lockfile);
            const auto error = maybe_lockfile.error();
            REQUIRE_EQ(mamba_error_code::env_lockfile_parsing_failed, error.error_code());
            const auto& error_details = EnvLockFileError::get_details(error);
            CHECK_EQ(file_parsing_error_code::parsing_failure, error_details.parsing_error_code);
        }

        TEST_CASE("valid_one_package_succeed")
        {
            ChannelContext channel_context{ Context::instance() };
            const fs::u8path lockfile_path{ test_data_dir
                                            / "env_lockfile/good_one_package-lock.yaml" };
            const auto maybe_lockfile = read_environment_lockfile(channel_context, lockfile_path);
            REQUIRE_MESSAGE(maybe_lockfile, maybe_lockfile.error().what());
            const auto lockfile = maybe_lockfile.value();
            CHECK_EQ(lockfile.get_all_packages().size(), 1);
        }

        TEST_CASE("valid_one_package_implicit_category")
        {
            ChannelContext channel_context{ Context::instance() };
            const fs::u8path lockfile_path{
                test_data_dir / "env_lockfile/good_one_package_missing_category-lock.yaml"
            };
            const auto maybe_lockfile = read_environment_lockfile(channel_context, lockfile_path);
            REQUIRE_MESSAGE(maybe_lockfile, maybe_lockfile.error().what());
            const auto lockfile = maybe_lockfile.value();
            CHECK_EQ(lockfile.get_all_packages().size(), 1);
        }

        TEST_CASE("valid_multiple_packages_succeed")
        {
            ChannelContext channel_context{ Context::instance() };
            const fs::u8path lockfile_path{ test_data_dir
                                            / "env_lockfile/good_multiple_packages-lock.yaml" };
            const auto maybe_lockfile = read_environment_lockfile(channel_context, lockfile_path);
            REQUIRE_MESSAGE(maybe_lockfile, maybe_lockfile.error().what());
            const auto lockfile = maybe_lockfile.value();
            CHECK_GT(lockfile.get_all_packages().size(), 1);
        }

        TEST_CASE("get_specific_packages")
        {
            ChannelContext channel_context{ Context::instance() };
            const fs::u8path lockfile_path{ test_data_dir
                                            / "env_lockfile/good_multiple_packages-lock.yaml" };
            const auto lockfile = read_environment_lockfile(channel_context, lockfile_path).value();
            CHECK(lockfile.get_packages_for("", "", "").empty());
            {
                const auto packages = lockfile.get_packages_for("main", "linux-64", "conda");
                CHECK_FALSE(packages.empty());
                CHECK_GT(packages.size(), 4);
            }
            {
                const auto packages = lockfile.get_packages_for("main", "linux-64", "pip");
                CHECK_FALSE(packages.empty());
                CHECK_EQ(packages.size(), 2);
            }
        }

        TEST_CASE("create_transaction_with_categories")
        {
            auto& ctx = Context::instance();
            const fs::u8path lockfile_path{ test_data_dir
                                            / "env_lockfile/good_multiple_categories-lock.yaml" };
            ChannelContext channel_context{ ctx };
            MPool pool{ channel_context };
            mamba::MultiPackageCache pkg_cache({ "/tmp/" }, ValidationOptions::from_context(ctx));

            ctx.platform = "linux-64";

            auto check_categories =
                [&](std::vector<std::string> categories, size_t num_conda, size_t num_pip)
            {
                std::vector<detail::other_pkg_mgr_spec> other_specs;
                auto transaction = create_explicit_transaction_from_lockfile(
                    pool,
                    lockfile_path,
                    categories,
                    pkg_cache,
                    other_specs
                );
                auto to_install = std::get<1>(transaction.to_conda());
                CHECK_EQ(to_install.size(), num_conda);
                if (num_pip == 0)
                {
                    CHECK_EQ(other_specs.size(), 0);
                }
                else
                {
                    CHECK_EQ(other_specs.size(), 1);
                    CHECK_EQ(other_specs.at(0).deps.size(), num_pip);
                }
            };

            check_categories({ "main" }, 3, 5);
            check_categories({ "main", "dev" }, 31, 6);
            check_categories({ "dev" }, 28, 1);
            check_categories({ "nonesuch" }, 0, 0);

            ctx.platform = ctx.host_platform;
        }
    }

    TEST_SUITE("is_env_lockfile_name")
    {
        TEST_CASE("basics")
        {
            CHECK(is_env_lockfile_name("something-lock.yaml"));
            CHECK(is_env_lockfile_name("something-lock.yml"));
            CHECK(is_env_lockfile_name("/some/dir/something-lock.yaml"));
            CHECK(is_env_lockfile_name("/some/dir/something-lock.yml"));
            CHECK(is_env_lockfile_name("../../some/dir/something-lock.yaml"));
            CHECK(is_env_lockfile_name("../../some/dir/something-lock.yml"));

            CHECK(is_env_lockfile_name(fs::u8path{ "something-lock.yaml" }.string()));
            CHECK(is_env_lockfile_name(fs::u8path{ "something-lock.yml" }.string()));
            CHECK(is_env_lockfile_name(fs::u8path{ "/some/dir/something-lock.yaml" }.string()));
            CHECK(is_env_lockfile_name(fs::u8path{ "/some/dir/something-lock.yml" }.string()));
            CHECK(is_env_lockfile_name(fs::u8path{ "../../some/dir/something-lock.yaml" }.string()));
            CHECK(is_env_lockfile_name(fs::u8path{ "../../some/dir/something-lock.yml" }.string()));

            CHECK_FALSE(is_env_lockfile_name("something"));
            CHECK_FALSE(is_env_lockfile_name("something-lock"));
            CHECK_FALSE(is_env_lockfile_name("/some/dir/something"));
            CHECK_FALSE(is_env_lockfile_name("../../some/dir/something"));

            CHECK_FALSE(is_env_lockfile_name("something.yaml"));
            CHECK_FALSE(is_env_lockfile_name("something.yml"));
            CHECK_FALSE(is_env_lockfile_name("/some/dir/something.yaml"));
            CHECK_FALSE(is_env_lockfile_name("/some/dir/something.yml"));
            CHECK_FALSE(is_env_lockfile_name("../../some/dir/something.yaml"));
            CHECK_FALSE(is_env_lockfile_name("../../some/dir/something.yml"));

            CHECK_FALSE(is_env_lockfile_name(fs::u8path{ "something" }.string()));
            CHECK_FALSE(is_env_lockfile_name(fs::u8path{ "something-lock" }.string()));
            CHECK_FALSE(is_env_lockfile_name(fs::u8path{ "/some/dir/something" }.string()));
            CHECK_FALSE(is_env_lockfile_name(fs::u8path{ "../../some/dir/something" }.string()));
        }
    }

}
