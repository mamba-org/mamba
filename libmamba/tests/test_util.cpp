#include <gtest/gtest.h>

#include "mamba/core/util.hpp"
#include "mamba/core/util_random.hpp"
#include "mamba/core/util_scope.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/context.hpp"


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

        EXPECT_TRUE(is_yaml_file_name(fs::path{ "something.yaml" }.string()));
        EXPECT_TRUE(is_yaml_file_name(fs::path{ "something.yml" }.string()));
        EXPECT_TRUE(is_yaml_file_name(fs::path{ "something-lock.yaml" }.string()));
        EXPECT_TRUE(is_yaml_file_name(fs::path{ "something-lock.yml" }.string()));
        EXPECT_TRUE(is_yaml_file_name(fs::path{ "/some/dir/something.yaml" }.string()));
        EXPECT_TRUE(is_yaml_file_name(fs::path{ "/some/dir/something.yml" }.string()));
        EXPECT_TRUE(is_yaml_file_name(fs::path{ "../../some/dir/something.yaml" }.string()));
        EXPECT_TRUE(is_yaml_file_name(fs::path{ "../../some/dir/something.yml" }.string()));

        EXPECT_FALSE(is_yaml_file_name("something"));
        EXPECT_FALSE(is_yaml_file_name("something-lock"));
        EXPECT_FALSE(is_yaml_file_name("/some/dir/something"));
        EXPECT_FALSE(is_yaml_file_name("../../some/dir/something"));

        EXPECT_FALSE(is_yaml_file_name(fs::path{ "something" }.string()));
        EXPECT_FALSE(is_yaml_file_name(fs::path{ "something-lock" }.string()));
        EXPECT_FALSE(is_yaml_file_name(fs::path{ "/some/dir/something" }.string()));
        EXPECT_FALSE(is_yaml_file_name(fs::path{ "../../some/dir/something" }.string()));
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

    TEST(utils, proxy_match)
    {
        Context::instance().proxy_servers = { { "http", "foo" },
                                              { "https", "bar" },
                                              { "https://example.net", "foobar" },
                                              { "https://example.net:8080", "baz" } };

        EXPECT_EQ(proxy_match("http://example.com/channel"), std::string("foo"));
        EXPECT_EQ(proxy_match("http:/example.net/channel"), std::string("foo"));
        EXPECT_EQ(proxy_match("https://example.com/channel"), std::string("bar"));
        EXPECT_EQ(proxy_match("https:/example.com:8080/channel"), std::string("bar"));
        EXPECT_EQ(proxy_match("https://example.net/channel"), std::string("foobar"));
        EXPECT_EQ(proxy_match("https://example.net:8080/channel"), std::string("baz"));
    }
}
