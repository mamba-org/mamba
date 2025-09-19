#define CATCH_CONFIG_MAIN

#include <catch2/catch_all.hpp>

#include <fmt/core.h>

#include <mamba/core/logging.hpp>

#include "test_logging_common.hpp"

namespace mamba::logging
{
    namespace
    {
        struct LogHandlerTestsResult
        {
            testing::Stats stats;
            AnyLogHandler handler;
        };

    }

    template <LogHandlerOrPtr T>
    auto test_classic_inline_logging_api_usage(T&& handler, size_t log_count) -> LogHandlerTestsResult
    {
        testing::Stats stats;

        //SECTION("start stop(manual) start")
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

        //SECTION("change parameters")
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

        //SECTION("logging")
        {
            for (size_t i = 0; i < log_count; ++i)
            {
                log({ .message = fmt::format("test log {}", i), .level = log_level::warn });
            }
            stats.log_count += log_count;
            stats.real_output_log_count += log_count;
        }

        //SECTION("backtrace")
        {
            static constexpr std::size_t arbitrary_backtrace_size = 5;
            enable_backtrace(arbitrary_backtrace_size);
            ++stats.backtrace_size_change_count;

            {
                for (size_t i = 0; i < log_count; ++i)
                {
                    log({ .message = fmt::format("test log in backtrace {}", i), .level = log_level::warn });
                }
                stats.log_count += log_count;

                log_backtrace();
                ++stats.backtrace_log_count;
                stats.real_output_log_count += std::min(arbitrary_backtrace_size, log_count);

                log_backtrace();
                ++stats.backtrace_log_count;

                log_backtrace();
                ++stats.backtrace_log_count;
            }

            {
                for (size_t i = 0; i < log_count; ++i)
                {
                    log({ .message = fmt::format("test log in backtrace without guards {}", i),
                          .level = log_level::warn });
                }
                stats.log_count += log_count;

                log_backtrace_no_guards();
                ++stats.backtrace_log_no_guard_count;
                stats.real_output_log_count += std::min(arbitrary_backtrace_size, log_count);

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

        //SECTION("flush")
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

    TEST_CASE("logging API basic tests")
    {
        SECTION("sunk log handler")
        {
            const auto results = test_classic_inline_logging_api_usage(testing::LogHandler_Tester{}, 42);
            REQUIRE(results.handler.has_value());
            REQUIRE(
                results.stats
                == results.handler.unsafe_get<testing::LogHandler_Tester>()->capture_stats()
            );
        }

        SECTION("pointer to movable log handler")
        {
            testing::LogHandler_Tester tester;
            const auto results = test_classic_inline_logging_api_usage(&tester, 96);
            REQUIRE(results.handler.has_value());
            REQUIRE(results.handler.unsafe_get<testing::LogHandler_Tester*>() == &tester);
            REQUIRE(results.stats == tester.capture_stats());
        }

        SECTION("supports pointer to non-movable log handlers")
        {
            testing::LogHandler_NotMovable not_movable;
            const auto results = test_classic_inline_logging_api_usage(&not_movable, 1234);
        }
    }

}
