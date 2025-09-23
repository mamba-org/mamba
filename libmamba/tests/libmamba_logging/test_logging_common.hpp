// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <random>
#include <thread>
#include <vector>

#include <catch2/catch_all.hpp>
#include <fmt/core.h>

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

    struct LogHandlerTestsOptions
    {
        std::size_t log_count = 10;
        std::string format_log_message = "test log {}";
        std::string format_log_message_backtrace = "test log in backtrace {}";
        std::string format_log_message_backtrace_without_guard = "test log in backtrace without guards {}";
        log_level level = log_level::warn;
        std::size_t backtrace_size = 5;
    };

    template <LogHandlerOrPtr T>
    auto test_classic_inline_logging_api_usage(T&& handler, const LogHandlerTestsOptions options = {})
        -> LogHandlerTestsResult
    {
        testing::Stats stats;

        // SECTION("start stop(manual) start")
        {
            auto previous_handler = set_log_handler(std::forward<T>(handler));
            REQUIRE(not previous_handler.has_value());
            REQUIRE(get_log_handler().has_value());
            ++stats.start_count;

            auto original_handler = stop_logging(stop_reason::manual_stop);
            REQUIRE(original_handler.has_value());
            REQUIRE(not get_log_handler().has_value());
            ++stats.stop_count;

            previous_handler = set_log_handler(std::move(original_handler));
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

        // SECTION("change parameters")
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

        // SECTION("logging")
        {
            for (size_t i = 0; i < options.log_count; ++i)
            {
                log({ .message = fmt::format(fmt::runtime(options.format_log_message), i),
                      .level = options.level });
            }
            stats.log_count += options.log_count;
            stats.real_output_log_count += options.log_count;
        }

        // SECTION("backtrace")
        {
            enable_backtrace(options.backtrace_size);
            ++stats.backtrace_size_change_count;

            {
                for (size_t i = 0; i < options.log_count; ++i)
                {
                    log({ .message = fmt::format(fmt::runtime(options.format_log_message_backtrace), i),
                          .level = options.level });
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
                          .level = options.level });
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

        // SECTION("flush")
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
        return { .stats = std::move(stats), .handler = stop_logging(stop_reason::program_exit) };
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
                log_impl_func({
                    .message = fmt::format(fmt::runtime(message_format), i),
                    .level = options.level,
                });
            }
        };

        output_loop(options.format_log_message);
        output_loop(options.format_log_message_backtrace, options.backtrace_size);
        output_loop(options.format_log_message_backtrace_without_guard, options.backtrace_size);
    };


}
