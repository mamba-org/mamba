// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <thread>

#include <catch2/catch_all.hpp>

#include "mamba/util/random.hpp"
#include "mamba/util/string.hpp"

using namespace mamba::util;

namespace
{
    TEST_CASE("local_random_generator")
    {
        auto same_thread_checks = []
        {
            auto& a = local_random_generator();
            auto& b = local_random_generator();
            REQUIRE(&a == &b);

            auto& c = local_random_generator<std::mt19937>();
            REQUIRE(&a == &c);

            auto& d = local_random_generator<std::mt19937_64>();
            REQUIRE(static_cast<void*>(&a) != static_cast<void*>(&d);

            return &a;
        };
        void* pointer_to_this_thread_rng = same_thread_checks();

        void* pointer_to_another_thread_rng = nullptr;
        std::thread another_thread{ [&] { pointer_to_another_thread_rng = same_thread_checks(); } };
        another_thread.join();

        REQUIRE(pointer_to_this_thread_rng != pointer_to_another_thread_rng);
    }

    TEST_CASE("value_in_range")
    {
        constexpr int arbitrary_min = -20;
        constexpr int arbitrary_max = 20;
        constexpr std::size_t attempts = 2000;
        for (std::size_t i = 0; i < attempts; ++i)
        {
            const int value = random_int(arbitrary_min, arbitrary_max);
            REQUIRE(value >= arbitrary_min);
            REQUIRE(value <= arbitrary_max);
        }
    }

    TEST_CASE("random_alphanumeric_string")
    {
        constexpr std::size_t attempts = 200;
        for (std::size_t i = 0; i < attempts; ++i)
        {
            const auto value = generate_random_alphanumeric_string(i);
            REQUIRE(value.size() == i);
            REQUIRE(std::all_of(
                value.cbegin(),
                value.cend(),
                [](char c) { return is_digit(c) || is_alpha(c); }
            ));
        }
    }
}
