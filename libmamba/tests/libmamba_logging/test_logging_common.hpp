// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <random>
#include <thread>
#include <vector>

#include <mamba/core/logging.hpp>
#include <mamba/util/synchronized_value.hpp>

namespace mamba::logging::testing
{
    struct NotALogHandler
    {
    };

    static_assert(not LogHandler<NotALogHandler>);

    struct Stats
    {
        std::size_t start_count = 0;
        std::size_t stop_count = 0;
        std::size_t log_count = 0;
        std::size_t log_level_change_count = 0;
        std::size_t params_change_count = 0;
        std::size_t backtrace_enabled_count = 0;
        std::size_t backtrace_disabled_count = 0;
        std::size_t backtrace_log_count = 0;
        std::size_t backtrace_log_no_guard_count = 0;
        std::size_t flush_all_count = 0;
        std::size_t flush_specific_source_count = 0;
        std::size_t flush_threshold_change_count = 0;

        LoggingParams current_params = {};
        std::size_t backtrace_size = 0;
        log_level flush_threshold = log_level::off;

        auto operator==(const Stats& other) const noexcept -> bool = default;
    };

    struct LogHandler_Tester
    {
        struct Impl
        {
            util::synchronized_value<Stats> stats;
        };

        std::unique_ptr<Impl> pimpl = std::make_unique<Impl>();

        LogHandler_Tester() = default;

        LogHandler_Tester(LogHandler_Tester&&) noexcept = default;
        LogHandler_Tester& operator=(LogHandler_Tester&&) noexcept = default;

        LogHandler_Tester(const LogHandler_Tester&) = delete;
        LogHandler_Tester& operator=(const LogHandler_Tester&) = delete;

        auto start_log_handling(LoggingParams params, const std::vector<log_source>&) -> void
        {
            auto stats = pimpl->stats.synchronize();
            ++stats->start_count;
            stats->current_params = std::move(params);
        }

        auto stop_log_handling(stop_reason) -> void
        {
            ++pimpl->stats->stop_count;
        }

        auto set_log_level(log_level new_level) -> void
        {
            auto stats = pimpl->stats.synchronize();
            ++stats->log_level_change_count;
            stats->current_params.logging_level = new_level;
        }

        auto set_params(LoggingParams new_params) -> void
        {
            auto stats = pimpl->stats.synchronize();
            ++stats->params_change_count;
            stats->current_params = std::move(new_params);
        }

        auto log(LogRecord) -> void
        {
            pimpl->stats->log_count++;
        }

        auto enable_backtrace(size_t record_buffer_size) -> void
        {
            auto stats = pimpl->stats.synchronize();
            ++stats->backtrace_enabled_count;
            stats->backtrace_size = record_buffer_size;
        }

        auto disable_backtrace() -> void
        {
            auto stats = pimpl->stats.synchronize();
            ++stats->backtrace_disabled_count;
            stats->backtrace_size = 0;
        }

        auto log_backtrace() -> void
        {
            ++pimpl->stats->backtrace_log_count;
        }

        auto log_backtrace_no_guards() -> void
        {
            ++pimpl->stats->backtrace_log_no_guard_count;
        }

        auto flush(std::optional<log_source> source = {}) -> void
        {
            if (source)
            {
                ++pimpl->stats->flush_specific_source_count;
            }
            else
            {
                ++pimpl->stats->flush_all_count;
            }
        }

        auto set_flush_threshold(log_level threshold_level) -> void
        {
            auto stats = pimpl->stats.synchronize();
            ++stats->flush_threshold_change_count;
            stats->flush_threshold = threshold_level;
        }
    };

    static_assert(LogHandler<LogHandler_Tester>);

    struct LogHandler_NotMovable
    {
        LogHandler_NotMovable() = default;
        LogHandler_NotMovable(const LogHandler_NotMovable&) = delete;
        LogHandler_NotMovable(LogHandler_NotMovable&&) = delete;
        LogHandler_NotMovable& operator=(const LogHandler_NotMovable&) = delete;
        LogHandler_NotMovable& operator=(LogHandler_NotMovable&&) = delete;

        // clang-format off
            auto start_log_handling(LoggingParams, const std::vector<log_source>&) -> void {}
            auto stop_log_handling(stop_reason) -> void {}
            auto set_log_level(log_level) -> void {}
            auto set_params(LoggingParams) -> void {}
            auto log(LogRecord) -> void {}
            auto enable_backtrace(size_t) -> void {}
            auto disable_backtrace() -> void {}
            auto log_backtrace() -> void {}
            auto log_backtrace_no_guards() -> void {}
            auto flush(std::optional<log_source> = {}) -> void {}
            auto set_flush_threshold(log_level) -> void {}

        // clang-format on
    };

    // Spawns a number of threads that will execute the provided task a given number of times.
    // This is useful to make sure there are great chances that the tasks
    // are being scheduled concurrently.
    // Joins all threads before exiting.
    // (extracted from test_execution.cpp then modified - TODO: factorize)
    template <typename Func>
    auto execute_tasks_from_concurrent_threads(
        std::size_t task_count,
        std::size_t tasks_per_thread,
        Func work_generator
    ) -> void
    {
        // const auto estimated_thread_count = (task_count / tasks_per_thread) * 2;
        // std::vector<std::thread> producers(estimated_thread_count);

        // std::size_t tasks_left_to_launch = task_count;
        // std::size_t thread_idx = 0;
        // while (tasks_left_to_launch > 0)
        //{
        //     const std::size_t tasks_to_generate = std::min(tasks_per_thread,
        //     tasks_left_to_launch); producers[thread_idx] = std::thread{ [=]
        //                                          {
        //                                              for (std::size_t i = 0; i <
        //                                              tasks_to_generate;
        //                                                   ++i)
        //                                              {
        //                                                  work(args...);
        //                                                  std::this_thread::yield();
        //                                              }
        //                                          } };
        //     tasks_left_to_launch -= tasks_to_generate;
        //     ++thread_idx;
        //     assert(thread_idx < producers.size());
        // }

        //// Make sure all the producers are finished before continuing.
        // for (auto&& t : producers)
        //{
        //     if (t.joinable())
        //     {
        //         t.join();
        //     }
        // }
    }

    template <LogHandler T>
    void test_heavy_concurrency()
    {
        // constexpr std::size_t arbitrary_operations_count = 2048;
        // constexpr std::size_t arbitrary_operations_per_generator = 24;

        // T log_handler;
        // log_handler.start_log_handling({}, all_log_sources());

        // struct RandomLoggingOp
        //{
        // };

        // execute_tasks_from_concurrent_threads(
        //     arbitrary_operations_count,
        //     arbitrary_operations_per_generator,
        //     // clang-format off
        //     [] {
        //         thread_local std::default_random_engine random_engine;
        //     }
        //     // clang-format on
        //)
    }
}
