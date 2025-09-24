// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <functional>
#include <random>
#include <thread>
#include <vector>

#include <catch2/catch_all.hpp>
#include <fmt/core.h>
#include <fmt/std.h>

#include <mamba/core/logging.hpp>
#include <mamba/core/util_scope.hpp>
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
        std::size_t real_output_log_count = 0;
        std::size_t log_level_change_count = 0;
        std::size_t params_change_count = 0;
        std::size_t backtrace_size_change_count = 0;
        std::size_t backtrace_log_count = 0;
        std::size_t backtrace_log_no_guard_count = 0;
        std::size_t flush_all_count = 0;
        std::size_t flush_specific_source_count = 0;
        std::size_t flush_threshold_change_count = 0;

        LoggingParams current_params = {};
        std::size_t backtrace_size = 0;
        std::size_t backtrace_logs_size = 0;
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

        Stats capture_stats() const
        {
            return pimpl->stats.value();
        }

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
            auto stats = pimpl->stats.synchronize();
            stats->log_count++;
            if (stats->backtrace_size == 0)
            {
                ++stats->real_output_log_count;
            }
            else
            {
                stats->backtrace_logs_size = std::min(
                    stats->backtrace_logs_size + 1,
                    stats->backtrace_size
                );
            }
        }

        auto enable_backtrace(size_t record_buffer_size) -> void
        {
            auto stats = pimpl->stats.synchronize();
            ++stats->backtrace_size_change_count;
            stats->backtrace_size = record_buffer_size;
        }

        auto log_backtrace() -> void
        {
            auto stats = pimpl->stats.synchronize();
            ++stats->backtrace_log_count;
            stats->real_output_log_count += stats->backtrace_logs_size;
            stats->backtrace_logs_size = 0;
        }

        auto log_backtrace_no_guards() -> void
        {
            auto stats = pimpl->stats.synchronize();
            ++stats->backtrace_log_no_guard_count;
            stats->real_output_log_count += stats->backtrace_logs_size;
            stats->backtrace_logs_size = 0;
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

    struct LogHandlerTestsResult
    {
        testing::Stats stats;
        AnyLogHandler handler;
    };

    inline auto testing_log_sources() -> std::vector<log_source>
    {
        return { log_source::tests };
    }

    struct LogHandlerTestsOptions
    {
        std::size_t log_count = 10;
        std::string format_log_message = "test log {}";
        std::string format_log_message_backtrace = "test log in backtrace {}";
        std::string format_log_message_backtrace_without_guard = "test log in backtrace without guards {}";
        log_level level = log_level::warn;
        std::size_t backtrace_size = 5;
        stop_reason last_stop_reason = stop_reason::program_exit;
        std::vector<log_source> log_sources = testing_log_sources();
    };

    template <LogHandlerOrPtr T>
    auto test_classic_inline_logging_api_usage(T&& handler, const LogHandlerTestsOptions options = {})
        -> LogHandlerTestsResult
    {
        if (options.log_sources.empty())
        {
            throw std::invalid_argument("at least one log source must be specified");
        }

        // clear previous log handler if any
        stop_logging(stop_reason::manual_stop);

        testing::Stats stats;

        // start stop(manual) start
        {
            auto previous_handler = set_log_handler(std::forward<T>(handler), {}, options.log_sources);
            REQUIRE(not previous_handler.has_value());
            REQUIRE(get_log_handler().has_value());
            ++stats.start_count;

            auto original_handler = stop_logging(stop_reason::manual_stop);
            REQUIRE(original_handler.has_value());
            REQUIRE(not get_log_handler().has_value());
            ++stats.stop_count;

            previous_handler = set_log_handler(std::move(original_handler), {}, options.log_sources);
            REQUIRE(not previous_handler.has_value());
            REQUIRE(get_log_handler().has_value());
            ++stats.start_count;
        }

        if constexpr (LogHandlerPtr<T>)
        {
            // T is a pointer, it should always be pointing to the original handler
            REQUIRE(get_log_handler().unsafe_get<T>() == handler);
        }

        // continue using the same handler in operations below
        REQUIRE(get_log_handler().has_value());

        // change parameters
        {
            set_log_level(log_level::debug);
            REQUIRE(get_log_level() == log_level::debug);
            ++stats.log_level_change_count;

            set_log_level(log_level::info);
            REQUIRE(get_log_level() == log_level::info);
            ++stats.log_level_change_count;

            const LoggingParams logging_params{ .logging_level = log_level::critical };
            set_logging_params(logging_params);
            REQUIRE(get_logging_params() == logging_params);
            ++stats.params_change_count;

            set_logging_params({});
            REQUIRE(get_logging_params() == LoggingParams{});
            ++stats.params_change_count;

            stats.current_params = get_logging_params();
        }

        // logging
        {
            for (size_t i = 0; i < options.log_count; ++i)
            {
                log({ .message = fmt::format(fmt::runtime(options.format_log_message), i),
                      .level = options.level,
                      .source = options.log_sources.front() });
            }
            stats.log_count += options.log_count;
            stats.real_output_log_count += options.log_count;
        }

        // backtrace
        {
            enable_backtrace(options.backtrace_size);
            ++stats.backtrace_size_change_count;

            {
                for (size_t i = 0; i < options.log_count; ++i)
                {
                    log({ .message = fmt::format(fmt::runtime(options.format_log_message_backtrace), i),
                          .level = options.level,
                          .source = options.log_sources.front() });
                }
                stats.log_count += options.log_count;

                log_backtrace();
                ++stats.backtrace_log_count;
                stats.real_output_log_count += std::min(options.backtrace_size, options.log_count);

                log_backtrace();
                ++stats.backtrace_log_count;

                log_backtrace();
                ++stats.backtrace_log_count;
            }

            {
                for (size_t i = 0; i < options.log_count; ++i)
                {
                    log({ .message = fmt::format(
                              fmt::runtime(options.format_log_message_backtrace_without_guard),
                              i
                          ),
                          .level = options.level,
                          .source = options.log_sources.front() });
                }
                stats.log_count += options.log_count;

                log_backtrace_no_guards();
                ++stats.backtrace_log_no_guard_count;
                stats.real_output_log_count += std::min(options.backtrace_size, options.log_count);

                log_backtrace_no_guards();
                ++stats.backtrace_log_no_guard_count;
                log_backtrace_no_guards();
                ++stats.backtrace_log_no_guard_count;
                log_backtrace_no_guards();
                ++stats.backtrace_log_no_guard_count;
            }

            disable_backtrace();
            ++stats.backtrace_size_change_count;

            disable_backtrace();
            ++stats.backtrace_size_change_count;
        }

        // flush
        {
            flush_logs();
            ++stats.flush_all_count;
            flush_logs();
            ++stats.flush_all_count;
            flush_logs();
            ++stats.flush_all_count;

            flush_logs(log_source::tests);
            ++stats.flush_specific_source_count;
            flush_logs(log_source::tests);
            ++stats.flush_specific_source_count;

            set_flush_threshold(log_level::all);
            ++stats.flush_threshold_change_count;
            stats.flush_threshold = log_level::all;
        }

        ++stats.stop_count;
        return { .stats = std::move(stats), .handler = stop_logging(options.last_stop_reason) };
    }

    // This generator must be kept in sync with testing::test_classic_inline_logging_api_usage()
    auto expected_output_test_classic_inline(
        std::invocable<LogRecord> auto log_impl_func,
        const testing::LogHandlerTestsOptions options
    ) -> void
    {
        const auto output_loop = [&](auto message_format, std::size_t backtrace_size = 0)
        {
            const std::size_t start_log_idx = [&]() -> std::size_t
            {
                if (backtrace_size == 0 or backtrace_size > options.log_count)
                {
                    return 0;
                }
                else
                {
                    return options.log_count - backtrace_size;
                }
            }();

            for (std::size_t i = start_log_idx; i < options.log_count; ++i)
            {
                log_impl_func({ .message = fmt::format(fmt::runtime(message_format), i),
                                .level = options.level,
                                .source = options.log_sources.front() });
            }
        };

        output_loop(options.format_log_message);
        output_loop(options.format_log_message_backtrace, options.backtrace_size);
        output_loop(options.format_log_message_backtrace_without_guard, options.backtrace_size);
    };

    struct Random
    {
        std::random_device r;
        std::seed_seq initial_seed{ r(), r(), r(), r(), r(), r(), r(), r() };
        std::mt19937 engine{ initial_seed };  // TODO C++26: use less memory-expensive philox
                                              // engines

        std::size_t roll_dice(std::size_t min, std::size_t max)
        {
            std::uniform_int_distribution dice{ min, max };
            return dice(engine);
        }
    };

    template <LogHandlerOrPtr T>
    auto test_concurrent_logging_api_support(
        T&& handler,
        const std::size_t runners_count = 123,
        const std::size_t max_operations_per_runner = 1234
    ) -> void
    {
        assert(runners_count > 0);
        assert(max_operations_per_runner > 0);

        set_log_handler(std::forward<T>(handler));
        on_scope_exit _{ [] { stop_logging(); } };

        REQUIRE(get_log_handler().has_value());

        std::atomic_bool green_light{ false };

        auto tasks = [&green_light, max_operations_per_runner]
        {
            auto random = std::make_unique<Random>();  // on the heap to avoid exploding stacks


            // clang-format off
            const auto random_log_level = [&] {
                return static_cast<log_level>(random->roll_dice(0, 5));
            };

            const auto random_log_source = [&] {
                static const auto sources = all_log_sources();
                return sources.at(random->roll_dice(0, sources.size() - 1));
            };

            const auto random_bactrace = [&] {
                return random->roll_dice(0,1) ? 0 : random->roll_dice(1, 50);
            };

            const auto random_params = [&] {
                return LoggingParams{
                    .logging_level = random_log_level(),
                    .log_backtrace = random_bactrace(),
                };
            };

            const auto random_log_record = [&]
            {
                return LogRecord
                {
                    .message = fmt::format("concurrent log thread {}", std::this_thread::get_id()),
                    .level = random_log_level(),
                    .source = random_log_source(),
                    .location = std::source_location::current(),
                };
            };

            using operation = std::function<void()>;

            std::vector<operation> operations {

                []{
                    REQUIRE(get_log_handler().has_value());
                },

                [&]{
                    set_log_level(random_log_level());
                },

                []{
                    const auto level = get_log_level();
                    REQUIRE(level != log_level::off);
                    REQUIRE(level != log_level::all);
                },

                [&]{
                    set_logging_params(random_params());
                },

                [&]{
                    enable_backtrace(random_bactrace());
                },

                [&]{
                    disable_backtrace();
                },

                [&]{
                    log_backtrace();
                },

                [&]{
                    log_backtrace_no_guards();
                },

                [&]{
                    flush_logs();
                },

                [&]{
                    flush_logs(random_log_source());
                },

                [&]{
                    const log_level selected_threshold = random->roll_dice(0, 1) ? log_level::off : random_log_level();
                    set_flush_threshold(selected_threshold);
                },
            };

            // many logs so that there are more chances to call these
            for (int idx = 0; idx < 20; ++idx)
            {
                operations.emplace_back([&]{
                    log(random_log_record());
                });
            }

            // clang-format on


            green_light.wait(false);  // wait for the green light


            std::size_t operations_left = random->roll_dice(
                std::min(std::size_t(100), max_operations_per_runner),
                max_operations_per_runner
            );

            while (operations_left != 0)
            {
                const auto idx = random->roll_dice(0, operations.size() - 1);
                auto& operation_to_run = operations.at(idx);

                operation_to_run();

                // introduce unpredictable delay between loops iterations
                if (random->roll_dice(0, 1))
                {
                    std::this_thread::yield();
                }

                --operations_left;
            }
        };

        std::vector<std::thread> runners;
        runners.reserve(runners_count);
        for (std::size_t idx = 0; idx < runners_count; ++idx)
        {
            runners.emplace_back(tasks);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        green_light = true;
        green_light.notify_all();  // all runners will now start

        // TODO: once C++20 library is properly supported by linux compilers,
        // use `jthread` instead of `thread` and remove the following loop.
        for (auto& thread : runners)
        {
            thread.join();
        }
    }

}
