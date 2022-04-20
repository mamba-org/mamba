#include <gtest/gtest.h>

#include <mamba/core/execution.hpp>

namespace mamba
{
    TEST(execution, stop_default_always_succeeds)
    {
        // This will also make sure that the following tests are not running
        MainExecutor::stop_default();
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
        const int arbitrary_task_count = 2048;
        const int arbitrary_tasks_per_generator = 24;
        std::atomic<int> counter{ 0 };
        {
            MainExecutor executor;

            // Spawn a number of threads generating tasks in the executor.
            // We do this from various threads instead of the current one
            // to make sure there are great chances that the tasks
            // are being scheduled concurrently.
            std::vector<std::thread> producers;
            int tasks_left_to_launch = arbitrary_task_count;
            while (tasks_left_to_launch > 0)
            {
                const int tasks_to_generate
                    = std::min(arbitrary_tasks_per_generator, tasks_left_to_launch);
                producers.emplace_back(
                    [&, tasks_to_generate]
                    {
                        for (int i = 0; i < tasks_to_generate; ++i)
                        {
                            executor.schedule([&] { ++counter; });
                        }
                    });
                tasks_left_to_launch -= tasks_to_generate;
            }

            for (auto&& t : producers)
                t.join();  // Make sure all the producers are finished before continuing.

        }  // All threads from the executor must have been joined here.
        EXPECT_EQ(counter, arbitrary_task_count);
    }
}
