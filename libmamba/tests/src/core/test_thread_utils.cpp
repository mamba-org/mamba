// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>

#include <catch2/catch_all.hpp>

#include "mamba/core/context.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/thread_utils.hpp"

#include "mambatests.hpp"

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
        // Ensures the compiler doe snot optimize away mambatests::context()
        std::string current_command = mambatests::context().command_params.current_command;
        CHECK_EQ(current_command, "mamba");
        Console::instance().init_progress_bar_manager(ProgressBarMode::multi);
        {
            interruption_guard g(
                [&res]()
                {
                    // Test for double free (segfault if that happens)
                    std::cout << "Interruption guard is interrupting" << std::endl;
                    Console::instance().init_progress_bar_manager(ProgressBarMode::multi);
                    {
                        std::unique_lock<std::mutex> lk(res_mutex);
                        res -= 100;
                    }
                    reset_sig_interrupted();
                }
            );

            for (size_t i = 0; i < 5; ++i)
            {
                MainExecutor::instance().take_ownership(mamba::thread{
                    [&res]
                    {
                        {
                            std::unique_lock<std::mutex> lk(res_mutex);
                            ++res;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(300));
                    } }.extract());
            }
            if (interrupt)
            {
                stop_receiver_thread();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        return res;
    }

    namespace
    {
        TEST_CASE("interrupt")
        {
            int res = test_interruption_guard(true);
            CHECK_EQ(res, -95);
        }

        TEST_CASE("no_interrupt")
        {
            int res = test_interruption_guard(false);
            CHECK_EQ(res, 5);
        }

        TEST_CASE("no_interrupt_then_interrupt")
        {
            int res = test_interruption_guard(false);
            CHECK_EQ(res, 5);
            int res2 = test_interruption_guard(true);
            CHECK_EQ(res2, -95);
        }

        TEST_CASE("no_interrupt_sequence")
        {
            int res = test_interruption_guard(false);
            CHECK_EQ(res, 5);
            int res2 = test_interruption_guard(false);
            CHECK_EQ(res2, 5);
        }
    }
#endif
}  // namespace mamba
