#include <gtest/gtest.h>

#include <thread>

#include "mamba/core/util.hpp"

namespace mamba
{
    TEST(local_random_generator, one_rng_per_thread_and_type)
    {
        auto same_thread_checks = []{
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
        std::thread another_thread{[&]{
            pointer_to_another_thread_rng = same_thread_checks();
        }};
        another_thread.join();

        EXPECT_NE(pointer_to_this_thread_rng, pointer_to_another_thread_rng);
    }
}


