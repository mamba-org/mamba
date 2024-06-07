// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/core/execution.hpp"

namespace mamba
{
    // Spawns a number of threads that will execute the provided task a given number of times.
    // This is useful to make sure there are great chances that the tasks
    // are being scheduled concurrently.
    // Joins all threads before exiting.
    template <typename Func>
    void
    execute_tasks_from_concurrent_threads(std::size_t task_count, std::size_t tasks_per_thread, Func work)
    {
        const auto estimated_thread_count = (task_count / tasks_per_thread) * 2;
        std::vector<std::thread> producers(estimated_thread_count);

        std::size_t tasks_left_to_launch = task_count;
        std::size_t thread_idx = 0;
        while (tasks_left_to_launch > 0)
        {
            const std::size_t tasks_to_generate = std::min(tasks_per_thread, tasks_left_to_launch);
            producers[thread_idx] = std::thread{ [=]
                                                 {
                                                     for (std::size_t i = 0; i < tasks_to_generate;
                                                          ++i)
                                                     {
                                                         work();
                                                     }
                                                 } };
            tasks_left_to_launch -= tasks_to_generate;
            ++thread_idx;
            assert(thread_idx < producers.size());
        }

        // Make sure all the producers are finished before continuing.
        for (auto&& t : producers)
        {
            if (t.joinable())
            {
                t.join();
            }
        }
    }

    TEST_SUITE("execution")
    {
        TEST_CASE("stop_default_always_succeeds")
        {
            MainExecutor::stop_default();  // Make sure no other default main executor is running.
            MainExecutor::instance();      // Make sure we use the default main executor.
            MainExecutor::stop_default();  // Stop the default main executor and make sure it's not
                                           // enabled for the following tests.
            MainExecutor::stop_default();  // However the number of time we call it it should never
                                           // fail.
        }

        TEST_CASE("manual_executor_construction_destruction")
        {
            MainExecutor executor;
        }

        TEST_CASE("two_main_executors_fails")
        {
            MainExecutor executor;
            REQUIRE_THROWS_AS(MainExecutor{}, MainExecutorError);
        }

        TEST_CASE("tasks_complete_before_destruction_ends")
        {
            constexpr std::size_t arbitrary_task_count = 2048;
            constexpr std::size_t arbitrary_tasks_per_generator = 24;
            std::atomic<int> counter{ 0 };
            {
                MainExecutor executor;

                execute_tasks_from_concurrent_threads(
                    arbitrary_task_count,
                    arbitrary_tasks_per_generator,
                    [&] { executor.schedule([&] { ++counter; }); }
                );
            }  // All threads from the executor must have been joined here.
            CHECK_EQ(counter, arbitrary_task_count);
        }

        TEST_CASE("closed_prevents_more_scheduling_and_joins")
        {
            constexpr std::size_t arbitrary_task_count = 2048;
            constexpr std::size_t arbitrary_tasks_per_generator = 36;
            std::atomic<int> counter{ 0 };
            {
                MainExecutor executor;

                execute_tasks_from_concurrent_threads(
                    arbitrary_task_count,
                    arbitrary_tasks_per_generator,
                    [&] { executor.schedule([&] { ++counter; }); }
                );

                executor.close();
                CHECK_EQ(counter, arbitrary_task_count);

                execute_tasks_from_concurrent_threads(
                    arbitrary_task_count,
                    arbitrary_tasks_per_generator,
                    [&] { executor.schedule([&] { throw "this code must never be executed"; }); }
                );
            }
            CHECK_EQ(
                counter,
                arbitrary_task_count
            );  // We re-check to make sure no thread are executed anymore
                // as soon as `.close()` was called.
        }
    }

}
