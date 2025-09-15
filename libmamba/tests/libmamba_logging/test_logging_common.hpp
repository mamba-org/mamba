// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <random>
#include <thread>
#include <vector>

#include <mamba/core/logging.hpp>

namespace mamba::logging::testing
{
    // Spawns a number of threads that will execute the provided task a given number of times.
    // This is useful to make sure there are great chances that the tasks
    // are being scheduled concurrently.
    // Joins all threads before exiting.
    // (extracted from test_execution.cpp then modified - TODO: factorize)
    template <typename Func>
    void execute_tasks_from_concurrent_threads(
        std::size_t task_count,
        std::size_t tasks_per_thread,
        Func work_generator
    )
    {
        const auto estimated_thread_count = (task_count / tasks_per_thread) * 2;
        std::vector<std::thread> producers(estimated_thread_count);

        std::size_t tasks_left_to_launch = task_count;
        std::size_t thread_idx = 0;
        while (tasks_left_to_launch > 0)
        {
            const std::size_t tasks_to_generate = std::min(tasks_per_thread, tasks_left_to_launch);
            producers[thread_idx] = std::thread{
                [=]{
                    for (std::size_t i = 0; i < tasks_to_generate;
                        ++i)
                    {
                        work(args...);
                        std::this_thread::yield();
                    }
                }
            };
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

    template <LogHandler T>
    void test_heavy_concurrency()
    {
        constexpr std::size_t arbitrary_operations_count = 2048;
        constexpr std::size_t arbitrary_operations_per_generator = 24;

        T log_handler;
        log_handler.start_log_handling({}, all_log_sources());

        struct RandomLoggingOp
        {
        };

        execute_tasks_from_concurrent_threads(
            arbitrary_operations_count,
            arbitrary_operations_per_generator,
            // clang-format off
            [] {
                thread_local std::default_random_engine random_engine;
            }
            // clang-format on
        )
    }
}
