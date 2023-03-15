// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <gtest/gtest.h>

#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_random.hpp"
#include "mamba/core/util_scope.hpp"


namespace mamba
{
    TEST(local_random_generator, one_rng_per_thread_and_type)
    {
        auto same_thread_checks = []
        {
            auto& a = local_random_generator();
            auto& b = local_random_generator();
            EXPECT_EQ(&a, &b);

            auto& c = local_random_generator<std::mt19937>();
            EXPECT_EQ(&a, &c);

            auto& d = local_random_generator<std::mt19937_64>();
            EXPECT_NE(static_cast<void*>(&a), static_cast<void*>(&d));

            return &a;
        };
        void* pointer_to_this_thread_rng = same_thread_checks();

        void* pointer_to_another_thread_rng = nullptr;
        std::thread another_thread{ [&] { pointer_to_another_thread_rng = same_thread_checks(); } };
        another_thread.join();

        EXPECT_NE(pointer_to_this_thread_rng, pointer_to_another_thread_rng);
    }

    TEST(random_int, value_in_range)
    {
        constexpr int arbitrary_min = -20;
        constexpr int arbitrary_max = 20;
        constexpr int attempts = 2000;
        for (int i = 0; i < attempts; ++i)
        {
            const int value = random_int(arbitrary_min, arbitrary_max);
            EXPECT_GE(value, arbitrary_min);
            EXPECT_LE(value, arbitrary_max);
        }
    }

    TEST(on_scope_exit, basics)
    {
        bool executed = false;
        {
            on_scope_exit _{ [&] { executed = true; } };
            EXPECT_FALSE(executed);
        }
        EXPECT_TRUE(executed);
    }

    TEST(is_yaml_file_name, basics)
    {
        EXPECT_TRUE(is_yaml_file_name("something.yaml"));
        EXPECT_TRUE(is_yaml_file_name("something.yml"));
        EXPECT_TRUE(is_yaml_file_name("something-lock.yaml"));
        EXPECT_TRUE(is_yaml_file_name("something-lock.yml"));
        EXPECT_TRUE(is_yaml_file_name("/some/dir/something.yaml"));
        EXPECT_TRUE(is_yaml_file_name("/some/dir/something.yaml"));
        EXPECT_TRUE(is_yaml_file_name("../../some/dir/something.yml"));
        EXPECT_TRUE(is_yaml_file_name("../../some/dir/something.yml"));

        EXPECT_TRUE(is_yaml_file_name(fs::u8path{ "something.yaml" }.string()));
        EXPECT_TRUE(is_yaml_file_name(fs::u8path{ "something.yml" }.string()));
        EXPECT_TRUE(is_yaml_file_name(fs::u8path{ "something-lock.yaml" }.string()));
        EXPECT_TRUE(is_yaml_file_name(fs::u8path{ "something-lock.yml" }.string()));
        EXPECT_TRUE(is_yaml_file_name(fs::u8path{ "/some/dir/something.yaml" }.string()));
        EXPECT_TRUE(is_yaml_file_name(fs::u8path{ "/some/dir/something.yml" }.string()));
        EXPECT_TRUE(is_yaml_file_name(fs::u8path{ "../../some/dir/something.yaml" }.string()));
        EXPECT_TRUE(is_yaml_file_name(fs::u8path{ "../../some/dir/something.yml" }.string()));

        EXPECT_FALSE(is_yaml_file_name("something"));
        EXPECT_FALSE(is_yaml_file_name("something-lock"));
        EXPECT_FALSE(is_yaml_file_name("/some/dir/something"));
        EXPECT_FALSE(is_yaml_file_name("../../some/dir/something"));

        EXPECT_FALSE(is_yaml_file_name(fs::u8path{ "something" }.string()));
        EXPECT_FALSE(is_yaml_file_name(fs::u8path{ "something-lock" }.string()));
        EXPECT_FALSE(is_yaml_file_name(fs::u8path{ "/some/dir/something" }.string()));
        EXPECT_FALSE(is_yaml_file_name(fs::u8path{ "../../some/dir/something" }.string()));
    }

    TEST(utils, encode_decode_base64)
    {
        for (std::size_t i = 1; i < 20; ++i)
        {
            for (std::size_t j = 0; j < 5; ++j)
            {
                std::string r = mamba::generate_random_alphanumeric_string(i);
                auto e = encode_base64(r);
                EXPECT_TRUE(e);
                auto x = decode_base64(e.value());
                EXPECT_TRUE(x);
                EXPECT_EQ(r, x.value());
            }
        }
    }

    TEST(fsutils, is_writable)
    {
        const auto test_dir_path = fs::temp_directory_path() / "libmamba" / "writable_tests";
        fs::create_directories(test_dir_path);
        on_scope_exit _{ [&]
                         {
                             fs::permissions(test_dir_path, fs::perms::all);
                             fs::remove_all(test_dir_path);
                         } };

        EXPECT_TRUE(path::is_writable(test_dir_path));
        fs::permissions(test_dir_path, fs::perms::none);
        EXPECT_FALSE(path::is_writable(test_dir_path));
        fs::permissions(test_dir_path, fs::perms::all);
        EXPECT_TRUE(path::is_writable(test_dir_path));

        EXPECT_TRUE(path::is_writable(test_dir_path / "non-existing-writable-test-delete-me.txt"));
        EXPECT_TRUE(path::is_writable(
            env::expand_user("~/.libmamba-non-existing-writable-test-delete-me.txt")
        ));

        {
            const auto existing_file_path = test_dir_path / "existing-writable-test-delete-me.txt";
            {
                std::ofstream temp_file{ existing_file_path.std_path() };
                ASSERT_TRUE(temp_file.is_open());
                temp_file << "delete me" << std::endl;
            }
            EXPECT_TRUE(path::is_writable(existing_file_path));
            fs::permissions(existing_file_path, fs::perms::none);
            EXPECT_FALSE(path::is_writable(existing_file_path));
            fs::permissions(existing_file_path, fs::perms::all);
            EXPECT_TRUE(path::is_writable(existing_file_path));
        }
    }

    TEST(utils, proxy_match)
    {
        Context::instance().proxy_servers = { { "http", "foo" },
                                              { "https", "bar" },
                                              { "https://example.net", "foobar" },
                                              { "all://example.net", "baz" },
                                              { "all", "other" } };

        EXPECT_EQ(*proxy_match("http://example.com/channel"), "foo");
        EXPECT_EQ(*proxy_match("http://example.net/channel"), "foo");
        EXPECT_EQ(*proxy_match("https://example.com/channel"), "bar");
        EXPECT_EQ(*proxy_match("https://example.com:8080/channel"), "bar");
        EXPECT_EQ(*proxy_match("https://example.net/channel"), "foobar");
        EXPECT_EQ(*proxy_match("ftp://example.net/channel"), "baz");
        EXPECT_EQ(*proxy_match("ftp://example.org"), "other");

        Context::instance().proxy_servers = { { "http", "foo" },
                                              { "https", "bar" },
                                              { "https://example.net", "foobar" },
                                              { "all://example.net", "baz" } };

        EXPECT_FALSE(proxy_match("ftp://example.org").has_value());

        Context::instance().proxy_servers = {};

        EXPECT_FALSE(proxy_match("http://example.com/channel").has_value());
    }
}
