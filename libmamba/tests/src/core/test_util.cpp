// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

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
    TEST_SUITE("local_random_generator")
    {
        TEST_CASE("one_rng_per_thread_and_type")
        {
            auto same_thread_checks = []
            {
                auto& a = local_random_generator();
                auto& b = local_random_generator();
                CHECK_EQ(&a, &b);

                auto& c = local_random_generator<std::mt19937>();
                CHECK_EQ(&a, &c);

                auto& d = local_random_generator<std::mt19937_64>();
                CHECK_NE(static_cast<void*>(&a), static_cast<void*>(&d));

                return &a;
            };
            void* pointer_to_this_thread_rng = same_thread_checks();

            void* pointer_to_another_thread_rng = nullptr;
            std::thread another_thread{ [&]
                                        { pointer_to_another_thread_rng = same_thread_checks(); } };
            another_thread.join();

            CHECK_NE(pointer_to_this_thread_rng, pointer_to_another_thread_rng);
        }
    }

    TEST_SUITE("random_int")
    {
        TEST_CASE("value_in_range")
        {
            constexpr int arbitrary_min = -20;
            constexpr int arbitrary_max = 20;
            constexpr int attempts = 2000;
            for (int i = 0; i < attempts; ++i)
            {
                const int value = random_int(arbitrary_min, arbitrary_max);
                REQUIRE_GE(value, arbitrary_min);
                REQUIRE_LE(value, arbitrary_max);
            }
        }
    }

    TEST_SUITE("on_scope_exit")
    {
        TEST_CASE("basics")
        {
            bool executed = false;
            {
                on_scope_exit _{ [&] { executed = true; } };
                CHECK_FALSE(executed);
            }
            CHECK(executed);
        }
    }

    TEST_SUITE("is_yaml_file_name")
    {
        TEST_CASE("basics")
        {
            CHECK(is_yaml_file_name("something.yaml"));
            CHECK(is_yaml_file_name("something.yml"));
            CHECK(is_yaml_file_name("something-lock.yaml"));
            CHECK(is_yaml_file_name("something-lock.yml"));
            CHECK(is_yaml_file_name("/some/dir/something.yaml"));
            CHECK(is_yaml_file_name("/some/dir/something.yaml"));
            CHECK(is_yaml_file_name("../../some/dir/something.yml"));
            CHECK(is_yaml_file_name("../../some/dir/something.yml"));

            CHECK(is_yaml_file_name(fs::u8path{ "something.yaml" }.string()));
            CHECK(is_yaml_file_name(fs::u8path{ "something.yml" }.string()));
            CHECK(is_yaml_file_name(fs::u8path{ "something-lock.yaml" }.string()));
            CHECK(is_yaml_file_name(fs::u8path{ "something-lock.yml" }.string()));
            CHECK(is_yaml_file_name(fs::u8path{ "/some/dir/something.yaml" }.string()));
            CHECK(is_yaml_file_name(fs::u8path{ "/some/dir/something.yml" }.string()));
            CHECK(is_yaml_file_name(fs::u8path{ "../../some/dir/something.yaml" }.string()));
            CHECK(is_yaml_file_name(fs::u8path{ "../../some/dir/something.yml" }.string()));

            CHECK_FALSE(is_yaml_file_name("something"));
            CHECK_FALSE(is_yaml_file_name("something-lock"));
            CHECK_FALSE(is_yaml_file_name("/some/dir/something"));
            CHECK_FALSE(is_yaml_file_name("../../some/dir/something"));

            CHECK_FALSE(is_yaml_file_name(fs::u8path{ "something" }.string()));
            CHECK_FALSE(is_yaml_file_name(fs::u8path{ "something-lock" }.string()));
            CHECK_FALSE(is_yaml_file_name(fs::u8path{ "/some/dir/something" }.string()));
            CHECK_FALSE(is_yaml_file_name(fs::u8path{ "../../some/dir/something" }.string()));
        }
    }

    TEST_SUITE("utils")
    {
        TEST_CASE("encode_decode_base64")
        {
            for (std::size_t i = 1; i < 20; ++i)
            {
                for (std::size_t j = 0; j < 5; ++j)
                {
                    std::string r = mamba::generate_random_alphanumeric_string(i);
                    auto e = encode_base64(r);
                    CHECK(e);
                    auto x = decode_base64(e.value());
                    CHECK(x);
                    CHECK_EQ(r, x.value());
                }
            }
        }
    }

    TEST_SUITE("fsutils")
    {
        TEST_CASE("is_writable")
        {
            const auto test_dir_path = fs::temp_directory_path() / "libmamba" / "writable_tests";
            fs::create_directories(test_dir_path);
            on_scope_exit _{ [&]
                             {
                                 fs::permissions(test_dir_path, fs::perms::all);
                                 fs::remove_all(test_dir_path);
                             } };

            CHECK(path::is_writable(test_dir_path));
            fs::permissions(test_dir_path, fs::perms::none);
            CHECK_FALSE(path::is_writable(test_dir_path));
            fs::permissions(test_dir_path, fs::perms::all);
            CHECK(path::is_writable(test_dir_path));

            CHECK(path::is_writable(test_dir_path / "non-existing-writable-test-delete-me.txt"));
            CHECK(path::is_writable(
                env::expand_user("~/.libmamba-non-existing-writable-test-delete-me.txt")
            ));

            CHECK(path::is_writable(test_dir_path / "non-existing-subfolder"));
            CHECK_FALSE(fs::exists(test_dir_path / "non-existing-subfolder"));

            {
                const auto existing_file_path = test_dir_path
                                                / "existing-writable-test-delete-me.txt";
                {
                    std::ofstream temp_file{ existing_file_path.std_path() };
                    REQUIRE(temp_file.is_open());
                    temp_file << "delete me" << std::endl;
                }
                CHECK(path::is_writable(existing_file_path));
                fs::permissions(existing_file_path, fs::perms::none);
                CHECK_FALSE(path::is_writable(existing_file_path));
                fs::permissions(existing_file_path, fs::perms::all);
                CHECK(path::is_writable(existing_file_path));
            }
        }
    }

    TEST_SUITE("utils")
    {
        TEST_CASE("proxy_match")
        {
            auto& context = Context::instance();
            context.remote_fetch_params.proxy_servers = {
                                    { "http", "foo" },
                                                        { "https", "bar" },
                                                        { "https://example.net",
                                                                        "foobar" },
                                                        { "all://example.net", "baz" },
                                                        { "all", "other" }
                                    };

            auto proxy_match_with_context = [&](const char* url){
                return proxy_match(url, context.remote_fetch_params.proxy_servers);
            };

            CHECK_EQ(*proxy_match_with_context("http://example.com/channel"), "foo");
            CHECK_EQ(*proxy_match_with_context("http://example.net/channel"), "foo");
            CHECK_EQ(*proxy_match_with_context("https://example.com/channel"), "bar");
            CHECK_EQ(*proxy_match_with_context("https://example.com:8080/channel"), "bar");
            CHECK_EQ(*proxy_match_with_context("https://example.net/channel"), "foobar");
            CHECK_EQ(*proxy_match_with_context("ftp://example.net/channel"), "baz");
            CHECK_EQ(*proxy_match_with_context("ftp://example.org"), "other");

            context.remote_fetch_params.proxy_servers = {
                { "http", "foo" },
                { "https", "bar" },
                { "https://example.net", "foobar" },
                { "all://example.net", "baz" }
            };

            CHECK_FALSE(proxy_match_with_context("ftp://example.org").has_value());

            context.remote_fetch_params.proxy_servers = {};

            CHECK_FALSE(proxy_match_with_context("http://example.com/channel").has_value());
        }
    }
}
