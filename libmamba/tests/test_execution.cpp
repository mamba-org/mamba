#include <gtest/gtest.h>

#include <mamba/core/execution.hpp>

namespace mamba
{
    // Spawns a number of threads that will execute the provided task a given number of times.
    // This is useful to make sure there are great chances that the tasks
    // are being scheduled concurrently.
    // Joins all threads before exiting.
    template <typename Func>
    void execute_tasks_from_concurrent_threads(size_t task_count,
                                               size_t tasks_per_thread,
                                               Func work)
    {
        std::vector<std::thread> producers;
        size_t tasks_left_to_launch = task_count;
        while (tasks_left_to_launch > 0)
        {
            const size_t tasks_to_generate = std::min(tasks_per_thread, tasks_left_to_launch);
            producers.emplace_back(
                [=]
                {
                    for (int i = 0; i < tasks_to_generate; ++i)
                    {
                        work();
                    }
                });
            tasks_left_to_launch -= tasks_to_generate;
        }

        for (auto&& t : producers)
            t.join();  // Make sure all the producers are finished before continuing.
    }

    TEST(execution, stop_default_always_succeeds)
    {
        MainExecutor::stop_default(); // Make sure no other default main executor is running.
        MainExecutor::instance();     // Make sure we use the defaut main executor.
        MainExecutor::stop_default(); // Stop the default main executor and make sure it's not enabled for the following tests.
        MainExecutor::stop_default(); // However the number of time we call it it should never fail.
    }


    TEST(execution, manual_executor_construction_destruction)
    {
        MainExecutor executor;
    }

    TEST(execution, two_main_executors_fails)
    {
        MainExecutor executor;
        ASSERT_THROW(MainExecutor{}, MainExecutorError);
    }

    TEST(execution, tasks_complete_before_destruction_ends)
    {
        const size_t arbitrary_task_count = 2048;
        const size_t arbitrary_tasks_per_generator = 24;
        std::atomic<int> counter{ 0 };
        {
            MainExecutor executor;

            execute_tasks_from_concurrent_threads(arbitrary_task_count,
                                                  arbitrary_tasks_per_generator,
                                                  [&] { executor.schedule([&] { ++counter; }); });
        }  // All threads from the executor must have been joined here.
        EXPECT_EQ(counter, arbitrary_task_count);
    }

    TEST(execution, closed_prevents_more_scheduling_and_joins)
    {
        const size_t arbitrary_task_count = 2048;
        const size_t arbitrary_tasks_per_generator = 36;
        std::atomic<int> counter{ 0 };
        {
            MainExecutor executor;

            execute_tasks_from_concurrent_threads(arbitrary_task_count,
                                                  arbitrary_tasks_per_generator,
                                                  [&] { executor.schedule([&] { ++counter; }); });

            executor.close();
            EXPECT_EQ(counter, arbitrary_task_count);

            execute_tasks_from_concurrent_threads(
                arbitrary_task_count,
                arbitrary_tasks_per_generator,
                [&] { executor.schedule([&] { throw "this code must never be executed"; }); });
        }
        EXPECT_EQ(counter,
                  arbitrary_task_count);  // We re-check to make sure no thread are executed anymore
                                          // as soon as `.close()` was called.
    }

}
