#include <gtest/gtest.h>

#include <yaml-cpp/exceptions.h>

#include "mamba/core/env_lockfile.hpp"
#include "mamba/core/fsutil.hpp"

namespace mamba
{
    TEST(env_lockfile, absent_file_fails)
    {
        const auto maybe_lockfile = read_environment_lockfile("this/file/does/not/exists");
        ASSERT_FALSE(maybe_lockfile);
        const auto error = maybe_lockfile.error();
        ASSERT_EQ(mamba_error_code::env_lockfile_parsing_failed, error.error_code());

        const auto& error_details = EnvLockFileError::get_details(error);
        EXPECT_EQ(file_parsing_error_code::parsing_failure, error_details.parsing_error_code);
        ASSERT_TRUE(error_details.yaml_error_type);
        const std::type_index bad_file_error_id{ typeid(YAML::BadFile) };
        EXPECT_EQ(bad_file_error_id, error_details.yaml_error_type.value());

        // NOTE: one could attempt to check if opening a file which is not an YAML file
        //       would fail. Unfortunately YAML parsers will accept any kind of file,
        //       and assume it is YAML or at worse a comment or raw string. So there
        //       is no good way to check that.
    }

    TEST(env_lockfile, invalid_version_fails)
    {
        const fs::path invalid_version_lockfile_path{ "env_lockfile_test/bad_version-lock.yaml" };
        const auto maybe_lockfile = read_environment_lockfile(invalid_version_lockfile_path);
        ASSERT_FALSE(maybe_lockfile);
        const auto error = maybe_lockfile.error();
        ASSERT_EQ(mamba_error_code::env_lockfile_parsing_failed, error.error_code());
        const auto& error_details = EnvLockFileError::get_details(error);
        EXPECT_EQ(file_parsing_error_code::unsuported_version, error_details.parsing_error_code);
    }

    TEST(env_lockfile, valid_no_package_succeed)
    {
        const fs::path lockfile_path{ "env_lockfile_test/good_no_package-lock.yaml" };
        const auto maybe_lockfile = read_environment_lockfile(lockfile_path);
        ASSERT_TRUE(maybe_lockfile) << maybe_lockfile.error().what();
        const auto lockfile = maybe_lockfile.value();
        EXPECT_TRUE(lockfile.get_all_packages().empty());
    }

    TEST(env_lockfile, invalid_package_fails)
    {
        const fs::path lockfile_path{ "env_lockfile_test/bad_package-lock.yaml" };
        const auto maybe_lockfile = read_environment_lockfile(lockfile_path);
        ASSERT_FALSE(maybe_lockfile);
        const auto error = maybe_lockfile.error();
        ASSERT_EQ(mamba_error_code::env_lockfile_parsing_failed, error.error_code());
        const auto& error_details = EnvLockFileError::get_details(error);
        EXPECT_EQ(file_parsing_error_code::parsing_failure, error_details.parsing_error_code);
    }

    TEST(env_lockfile, valid_one_package_succeed)
    {
        const fs::path lockfile_path{ "env_lockfile_test/good_one_package-lock.yaml" };
        const auto maybe_lockfile = read_environment_lockfile(lockfile_path);
        ASSERT_TRUE(maybe_lockfile) << maybe_lockfile.error().what();
        const auto lockfile = maybe_lockfile.value();
        EXPECT_EQ(lockfile.get_all_packages().size(), 1);
    }

    TEST(env_lockfile, valid_multiple_packages_succeed)
    {
        const fs::path lockfile_path{ "env_lockfile_test/good_multiple_packages-lock.yaml" };
        const auto maybe_lockfile = read_environment_lockfile(lockfile_path);
        ASSERT_TRUE(maybe_lockfile) << maybe_lockfile.error().what();
        const auto lockfile = maybe_lockfile.value();
        EXPECT_GT(lockfile.get_all_packages().size(), 1);
    }

    TEST(env_lockfile, get_specific_packages)
    {
        const fs::path lockfile_path{ "env_lockfile_test/good_multiple_packages-lock.yaml" };
        const auto lockfile = read_environment_lockfile(lockfile_path).value();
        EXPECT_TRUE(lockfile.get_packages_for("", "", "").empty());
        {
            const auto packages = lockfile.get_packages_for("main", "linux-64", "conda");
            EXPECT_FALSE(packages.empty());
            EXPECT_GT(packages.size(), 4);
        }
    }


    TEST(is_env_lockfile_name, basics)
    {
        EXPECT_TRUE(is_env_lockfile_name("something-lock.yaml"));
        EXPECT_TRUE(is_env_lockfile_name("something-lock.yml"));
        EXPECT_TRUE(is_env_lockfile_name("/some/dir/something-lock.yaml"));
        EXPECT_TRUE(is_env_lockfile_name("/some/dir/something-lock.yml"));
        EXPECT_TRUE(is_env_lockfile_name("../../some/dir/something-lock.yaml"));
        EXPECT_TRUE(is_env_lockfile_name("../../some/dir/something-lock.yml"));

        EXPECT_TRUE(is_env_lockfile_name(fs::path{ "something-lock.yaml" }.string()));
        EXPECT_TRUE(is_env_lockfile_name(fs::path{ "something-lock.yml" }.string()));
        EXPECT_TRUE(is_env_lockfile_name(fs::path{ "/some/dir/something-lock.yaml" }.string()));
        EXPECT_TRUE(is_env_lockfile_name(fs::path{ "/some/dir/something-lock.yml" }.string()));
        EXPECT_TRUE(
            is_env_lockfile_name(fs::path{ "../../some/dir/something-lock.yaml" }.string()));
        EXPECT_TRUE(is_env_lockfile_name(fs::path{ "../../some/dir/something-lock.yml" }.string()));

        EXPECT_FALSE(is_env_lockfile_name("something"));
        EXPECT_FALSE(is_env_lockfile_name("something-lock"));
        EXPECT_FALSE(is_env_lockfile_name("/some/dir/something"));
        EXPECT_FALSE(is_env_lockfile_name("../../some/dir/something"));

        EXPECT_FALSE(is_env_lockfile_name("something.yaml"));
        EXPECT_FALSE(is_env_lockfile_name("something.yml"));
        EXPECT_FALSE(is_env_lockfile_name("/some/dir/something.yaml"));
        EXPECT_FALSE(is_env_lockfile_name("/some/dir/something.yml"));
        EXPECT_FALSE(is_env_lockfile_name("../../some/dir/something.yaml"));
        EXPECT_FALSE(is_env_lockfile_name("../../some/dir/something.yml"));

        EXPECT_FALSE(is_env_lockfile_name(fs::path{ "something" }.string()));
        EXPECT_FALSE(is_env_lockfile_name(fs::path{ "something-lock" }.string()));
        EXPECT_FALSE(is_env_lockfile_name(fs::path{ "/some/dir/something" }.string()));
        EXPECT_FALSE(is_env_lockfile_name(fs::path{ "../../some/dir/something" }.string()));
    }

}
