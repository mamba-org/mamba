// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_EXECUTION_HPP
#define MAMBA_CORE_EXECUTION_HPP

#include <atomic>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

#include "mamba/core/error_handling.hpp"
#include "mamba/util/synchronized_value.hpp"

namespace mamba
{
    struct MainExecutorError : public mamba_error
    {
        using mamba_error::mamba_error;
    };

    // Main execution resource (for example threads) handler for this library.
    // Allows scoping the lifetime of threads being used by the library.
    // The user code can either create an instance of this type to determine
    // itself the lifetime of the threads, or it can just use `MainExecutor::instance()`
    // to obtain a global static instance. In this last case, `MainExecutor::instance().close()`
    // have to be called before the end of `main()` to avoid undefined behaviors.
    // WARNING: this is a temporary solution designed to evolve, the current implementation
    // uses threads directly, a future implementation will use a thread-pool or other similar
    // mechanisms.
    class MainExecutor
    {
    public:

        // Set itself as the main executor.
        // Throws `MainExecutorError` if another instance already exists.
        MainExecutor();

        // Closes (see `close()`) and unregister itself as the main executor.
        // Blocks until all scheduled tasks are done and all resources are released (threads
        // joined).
        ~MainExecutor();

        // Returns a reference to the current main executor.
        // If no main executor have been set previously to this call,
        // a global one is created and returned. In this case the user must
        // call `MainExecutor::instance().close()` before the end of `main()` to avoid
        // undefined behaviors.
        static MainExecutor& instance();

        // If the default (global) main executor is being used, close and destroy it.
        // Do nothing otherwise.
        // This is mostly used for testing and libraries using the default main executor.
        static void stop_default();

        // Schedules a task for execution.
        // The task must be a callable which takes either the provided arguments or none.
        // If this executor is open, the task is scheduled for execution and will be called
        // as soon as execution resources are available. The call to the task is not guaranteed
        // to have been done at the end of the execution of this function, nor before.
        // If this executor is closed, the task is ignored and no code will be executed nor the task
        // be called.
        template <typename Task, typename... Args>
        void schedule(Task&& task, Args&&... args)
        {
            if (!is_open)
            {
                return;
            }

            auto synched_threads = threads.synchronize();
            if (is_open)  // Double check necessary for correctness
            {
                synched_threads->emplace_back(std::forward<Task>(task), std::forward<Args>(args)...);
            }
        }

        // Moves ownership of a thread into this executor.
        // This is used in case a thread needs to be manipulated in a particular way,
        // but we still want to avoid having to use `std::thread::detach()`. By
        // transferring the ownership of the thread to this executor, we are guaranteed that
        // the thread will be joined before the end of the lifetime of this executor.
        // If this executor is closed, no code will be executed and the thread will be destroyed,
        // resulting in a call to `std::terminate()` if the thread is not already joined.
        void take_ownership(std::thread thread)
        {
            if (!thread.joinable() || !is_open)
            {
                return;
            }

            auto synched_threads = threads.synchronize();
            if (is_open)  // Double check necessary for correctness
            {
                synched_threads->push_back(std::move(thread));
            }
        }

        // Closes this executor:
        // Only returns once all tasks scheduled before this call are finished
        // and all owned execution resources (aka threads) are released.
        // Note that if any task never ends, this function will never end either.
        // Once called this function makes all other functions no-op, even before returning, to
        // prevent running tasks from scheduling more tasks to run. This is should be used to
        // manually determine the lifetime of the executor's resources.
        void close()
        {
            bool expected = true;
            if (!is_open.compare_exchange_strong(expected, false))
            {
                return;
            }

            invoke_close_handlers();

            auto synched_threads = threads.synchronize();
            for (auto&& t : *synched_threads)
            {
                t.join();
            }
            synched_threads->clear();
        }

        using on_close_handler = std::function<void()>;

        void on_close(on_close_handler handler)
        {
            if (!is_open)
            {
                return;
            }

            auto handlers = close_handlers.synchronize();
            if (is_open)  // Double check needed to avoid adding new handles while closing.
            {
                handlers->push_back(std::move(handler));
            }
        }

    private:

        std::atomic<bool> is_open{ true };
        using Threads = std::vector<std::thread>;
        using CloseHandlers = std::vector<on_close_handler>;
        util::synchronized_value<Threads, std::recursive_mutex> threads;
        util::synchronized_value<CloseHandlers, std::recursive_mutex> close_handlers;

        void invoke_close_handlers();
    };


}

#endif
