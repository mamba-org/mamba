#include <gtest/gtest.h>

#include "context.hpp"
#include "output.hpp"
#include "thread_utils.hpp"

namespace mamba
{
    TEST(thread_utils, interrupt)
    {
#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
        int res = 0;
        // Ensures the compiler doe snot optimize away Context::instance()
        std::string current_command = Context::instance().current_command;
        EXPECT_EQ(current_command, "mamba");

        Console::instance().init_multi_progress();
        { 
        interruption_guard g([&res]()
        {
            // Test for double free (segfault if that happends)
            Console::instance().init_multi_progress();
            res = -1;
        });

        for (size_t i = 0; i < 5; ++i)
        {
            thread t([&res]()
            {
                ++res;
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            });
            t.detach();
        }

        pthread_kill(get_cleaning_thread_id(), SIGINT);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        EXPECT_EQ(res, -1);
#endif 
    }
}

