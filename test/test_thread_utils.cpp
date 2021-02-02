#include <gtest/gtest.h>

#include "mamba/context.hpp"
#include "mamba/output.hpp"
#include "mamba/thread_utils.hpp"

namespace mamba
{
    namespace
    {
        std::mutex res_mutex;
    }
#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
    int test_interruption_guard(bool interrupt)
    {
        
        int res = 0;
        // Ensures the compiler doe snot optimize away Context::instance()
        std::string current_command = Context::instance().current_command;
        EXPECT_EQ(current_command, "mamba");

        Console::instance().init_multi_progress();
        {
            interruption_guard g([&res]() {
                // Test for double free (segfault if that happends)
                Console::instance().init_multi_progress();
                res = -1;
            });

            for (size_t i = 0; i < 5; ++i)
            {
                thread t([&res]() {
                    {
                        std::unique_lock<std::mutex> lk(res_mutex);
                        ++res;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(300));
                });
                t.detach();
            }

            if (interrupt)
            {
                pthread_kill(get_cleaning_thread_id(), SIGINT);
            }
            while (get_thread_count() != 0)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        return res;
    }

    TEST(thread_utils, interrupt)
    {
        int res = test_interruption_guard(true);
        EXPECT_EQ(res, -1);
    }

    TEST(thread_utils, no_interrupt)
    {
        int res = test_interruption_guard(false);
        EXPECT_EQ(res, 5);
    }

    TEST(thread_utils, no_interrupt_then_interrupt)
    {
        int res1 = test_interruption_guard(false);
        EXPECT_EQ(res1, 5);
        int res2 = test_interruption_guard(true);
        EXPECT_EQ(res2, -1);
    }

    TEST(thread_utils, no_interrupt_sequence)
    {
        int res1 = test_interruption_guard(false);
        EXPECT_EQ(res1, 5);
        int res2 = test_interruption_guard(false);
        EXPECT_EQ(res2, 5);
    }
#endif
}  // namespace mamba
