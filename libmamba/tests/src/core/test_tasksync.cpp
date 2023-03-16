#include <future>
#include <thread>
#include <type_traits>

#include <gtest/gtest.h>

#include "mamba/core/tasksync.hpp"

namespace mamba
{
    // WARNING: this file will be moved to xtl as soon as possible, do not rely on it's existence
    // here!

    TEST(task_synchronizer, sync_types_never_move)
    {
        static_assert(std::is_copy_constructible<TaskSynchronizer>::value == false, "");
        static_assert(std::is_copy_assignable<TaskSynchronizer>::value == false, "");
        static_assert(std::is_move_constructible<TaskSynchronizer>::value == false, "");
        static_assert(std::is_move_assignable<TaskSynchronizer>::value == false, "");
    }


    TEST(task_synchronizer, no_task_no_problem)
    {
        TaskSynchronizer task_sync;
        task_sync.join_tasks();
    }

    TEST(task_synchronizer, tasks_are_joined_after_join_not_after_reset)
    {
        TaskSynchronizer task_sync;
        EXPECT_FALSE(task_sync.is_joined());

        task_sync.join_tasks();
        EXPECT_TRUE(task_sync.is_joined());

        task_sync.reset();
        EXPECT_FALSE(task_sync.is_joined());

        task_sync.join_tasks();
        EXPECT_TRUE(task_sync.is_joined());
    }

    namespace
    {
        void fail_now()
        {
            throw "this code should never be executed";
        }
    }

    TEST(task_synchronizer, once_joined_tasks_are_noop)
    {
        TaskSynchronizer task_sync;
        task_sync.join_tasks();
        EXPECT_TRUE(task_sync.is_joined());

        task_sync.join_tasks();  // nothing happen if we call it twice
        EXPECT_TRUE(task_sync.is_joined());

        auto no_op = task_sync.synchronized([] { fail_now(); });
        no_op();
    }


    TEST(task_synchronizer, unexecuted_synched_task_never_blocks_join)
    {
        TaskSynchronizer task_sync;
        auto synched_task = task_sync.synchronized([] { fail_now(); });
        task_sync.join_tasks();
        synched_task();  // noop
    }

    TEST(task_synchronizer, finished_synched_task_never_blocks_join)
    {
        int execution_count = 0;
        TaskSynchronizer task_sync;
        auto synched_task = task_sync.synchronized([&] { ++execution_count; });
        EXPECT_EQ(execution_count, 0);

        synched_task();
        EXPECT_EQ(execution_count, 1);

        task_sync.join_tasks();
        EXPECT_EQ(execution_count, 1);

        synched_task();
        EXPECT_EQ(execution_count, 1);
    }

    TEST(task_synchronizer, executed_synched_task_never_blocks_join)
    {
        int execution_count = 0;
        TaskSynchronizer task_sync;

        auto synched_task = task_sync.synchronized([&] { ++execution_count; });
        auto task_future = std::async(std::launch::async, synched_task);
        task_future.wait();

        task_sync.join_tasks();

        synched_task();

        EXPECT_EQ(execution_count, 1);
    }

    namespace
    {
        template <typename Predicate>
        void wait_condition(Predicate&& predicate)
        {
            while (!std::invoke(std::forward<Predicate>(predicate)))
            {
                std::this_thread::yield();
            }
        }
    }

    TEST(task_synchronizer, executing_synched_task_always_block_join)
    {
        std::string sequence;
        TaskSynchronizer task_sync;

        const auto unlock_duration = std::chrono::seconds{ 1 };
        std::atomic<bool> task_started{ false };
        std::atomic<bool> task_continue{ false };
        std::atomic<bool> unlocker_ready{ false };
        std::atomic<bool> unlocker_start{ false };

        auto ft_task = std::async(
            std::launch::async,
            task_sync.synchronized(
                [&]
                {
                    sequence.push_back('A');
                    task_started = true;
                    wait_condition([&] { return task_continue.load(); });
                    sequence.push_back('F');
                }
            )
        );

        wait_condition([&] { return task_started.load(); });
        EXPECT_EQ(sequence, "A");

        auto ft_unlocker = std::async(
            std::launch::async,
            task_sync.synchronized(
                [&]
                {
                    sequence.push_back('B');
                    unlocker_ready = true;
                    wait_condition([&] { return unlocker_start.load(); });
                    sequence.push_back('D');
                    std::this_thread::sleep_for(unlock_duration);  // Make sure the time is long
                                                                   // enough for joining to happen
                                                                   // only after.
                    sequence.push_back('E');
                    task_continue = true;
                }
            )
        );

        wait_condition([&] { return unlocker_ready.load(); });
        EXPECT_EQ(sequence, "AB");

        sequence.push_back('C');

        const auto begin_time = std::chrono::high_resolution_clock::now();
        std::atomic_signal_fence(std::memory_order_acq_rel);  // prevents the compiler from
                                                              // reordering

        unlocker_start = true;
        task_sync.join_tasks();

        std::atomic_signal_fence(std::memory_order_acq_rel);  // prevents the compiler from
                                                              // reordering
        const auto end_time = std::chrono::high_resolution_clock::now();

        EXPECT_EQ(sequence, "ABCDEF");
        EXPECT_GE(end_time - begin_time, unlock_duration);
    }

    TEST(task_synchronizer, throwing_task_never_block_join)
    {
        TaskSynchronizer task_sync;

        auto synched_task = task_sync.synchronized([] { throw 42; });
        auto task_future = std::async(std::launch::async, synched_task);
        task_future.wait();

        task_sync.join_tasks();

        synched_task();

        EXPECT_THROW(task_future.get(), int);
    }

}
