// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <deque>

#include <catch2/catch_all.hpp>
#include <fmt/core.h>

#include <mamba/core/logging_tools.hpp>

#include "test_logging_common.hpp"

// TODO:
// - specific tests for LogHandler_History
// - specific tests for LogHandler_StdOut


namespace mamba::logging
{
    TEST_CASE("details::queue_push")
    {
        const std::deque<LogRecord> initial_queue{ { .message = "A" },
                                                   { .message = "B" },
                                                   { .message = "C" } };

        const std::vector<LogRecord> new_logs{
            { .message = "X" },
            { .message = "Y" },
            { .message = "Z" },
        };

        SECTION("pushing logs in unbounded queue")
        {
            auto queue = initial_queue;

            const std::deque<LogRecord> expected{
                { .message = "A" }, { .message = "B" }, { .message = "C" },
                { .message = "X" }, { .message = "Y" }, { .message = "Z" },
            };

            for (const auto& log : new_logs)
            {
                details::queue_push(queue, 0, log);
            }

            REQUIRE(queue == expected);
        }

        SECTION("pushing logs in bounded queue")
        {
            const size_t queue_size = 4;

            auto queue = initial_queue;

            const std::deque<LogRecord> expected{
                { .message = "C" },
                { .message = "X" },
                { .message = "Y" },
                { .message = "Z" },
            };

            for (const auto& log : new_logs)
            {
                details::queue_push(queue, queue_size, log);
            }

            REQUIRE(queue == expected);
        }
    }

    TEST_CASE("details::BasicBacktrace")
    {
        LogRecord log_not_pushed{ .message = "must not be pushed" };
        LogRecord log_a{ .message = "A" };
        LogRecord log_b{ .message = "B" };
        LogRecord log_c{ .message = "C" };
        LogRecord log_d{ .message = "D" };
        LogRecord log_e{ .message = "E" };
        LogRecord log_f{ .message = "F" };
        LogRecord log_g{ .message = "G" };

        details::BasicBacktrace b;
        REQUIRE(not b.is_enabled());
        REQUIRE(std::distance(b.begin(), b.end()) == 0);
        REQUIRE(b.size() == 0);
        REQUIRE(b.empty());

        b.push_if_enabled(log_a);
        REQUIRE(not b.is_enabled());
        REQUIRE(std::distance(b.begin(), b.end()) == 0);
        REQUIRE(b.size() == 0);
        REQUIRE(b.empty());
        REQUIRE(not log_a.message.empty());

        b.set_max_trace(2);
        REQUIRE(b.is_enabled());
        REQUIRE(std::distance(b.begin(), b.end()) == 0);
        REQUIRE(b.size() == 0);
        REQUIRE(b.empty());

        b.push_if_enabled(log_a);
        REQUIRE(b.is_enabled());
        REQUIRE(std::distance(b.begin(), b.end()) == 1);
        REQUIRE(b.size() == 1);
        REQUIRE(not b.empty());
        REQUIRE(b.begin()->message == "A");
        REQUIRE(std::next(b.end(), -1)->message == "A");
        REQUIRE(log_a.message.empty());

        b.push_if_enabled(log_b);
        REQUIRE(b.is_enabled());
        REQUIRE(std::distance(b.begin(), b.end()) == 2);
        REQUIRE(b.size() == 2);
        REQUIRE(not b.empty());
        REQUIRE(b.begin()->message == "A");
        REQUIRE(std::next(b.end(), -1)->message == "B");
        REQUIRE(log_b.message.empty());

        b.push_if_enabled(log_c);
        REQUIRE(b.is_enabled());
        REQUIRE(std::distance(b.begin(), b.end()) == 2);
        REQUIRE(b.size() == 2);
        REQUIRE(not b.empty());
        REQUIRE(b.begin()->message == "B");
        REQUIRE(std::next(b.end(), -1)->message == "C");
        REQUIRE(log_b.message.empty());

        b.clear();
        REQUIRE(b.is_enabled());
        REQUIRE(std::distance(b.begin(), b.end()) == 0);
        REQUIRE(b.size() == 0);
        REQUIRE(b.empty());

        b.push_if_enabled(log_d);
        REQUIRE(b.is_enabled());
        REQUIRE(std::distance(b.begin(), b.end()) == 1);
        REQUIRE(b.size() == 1);
        REQUIRE(not b.empty());
        REQUIRE(b.begin()->message == "D");
        REQUIRE(std::next(b.end(), -1)->message == "D");
        REQUIRE(log_d.message.empty());

        b.push_if_enabled(log_e);
        REQUIRE(b.is_enabled());
        REQUIRE(std::distance(b.begin(), b.end()) == 2);
        REQUIRE(b.size() == 2);
        REQUIRE(not b.empty());
        REQUIRE(b.begin()->message == "D");
        REQUIRE(std::next(b.end(), -1)->message == "E");
        REQUIRE(log_e.message.empty());


        b.push_if_enabled(log_f);
        REQUIRE(b.is_enabled());
        REQUIRE(std::distance(b.begin(), b.end()) == 2);
        REQUIRE(b.size() == 2);
        REQUIRE(not b.empty());
        REQUIRE(b.begin()->message == "E");
        REQUIRE(std::next(b.end(), -1)->message == "F");
        REQUIRE(log_f.message.empty());

        b.disable();
        REQUIRE(not b.is_enabled());
        REQUIRE(std::distance(b.begin(), b.end()) == 0);
        REQUIRE(b.size() == 0);
        REQUIRE(b.empty());

        b.push_if_enabled(log_g);
        REQUIRE(not b.is_enabled());
        REQUIRE(std::distance(b.begin(), b.end()) == 0);
        REQUIRE(b.size() == 0);
        REQUIRE(b.empty());
        REQUIRE(not log_g.message.empty());
    }

    TEST_CASE("details::log_to_stream")
    {
        std::stringstream out;
        const auto location = std::source_location::current();
        const auto location_str = fmt::format(" ({})", details::as_log(location));

        details::log_to_stream(
            out,
            LogRecord{ .message = "this is a test",
                       .level = log_level::debug,
                       .source = log_source::tests,
                       .location = location },
            { .with_location = true }
        );

        const auto expected_log = fmt::format("\ndebug tests{} : this is a test", location_str);
        REQUIRE(out.str() == expected_log);
    }

    TEST_CASE("LogHandler_History basics")
    {
        static constexpr LogRecord any_log{ .message = "this is a test", .level = log_level::warn };

        LogHandler_History handler;
        REQUIRE(not handler.is_started());
        handler.start_log_handling({}, {});  // must be started to work
        REQUIRE(handler.is_started());


        SECTION("start and stop (manual)")
        {
            handler.start_log_handling({}, {});
            REQUIRE(handler.is_started());

            handler.stop_log_handling(stop_reason::manual_stop);
            REQUIRE(not handler.is_started());
        }

        handler.start_log_handling({}, {});
        REQUIRE(handler.is_started());

        SECTION("history")
        {
            REQUIRE(handler.capture_history().empty());

            handler.log(any_log);
            REQUIRE(handler.is_started());
            REQUIRE(handler.capture_history() == std::vector<LogRecord>{ any_log });

            handler.log(any_log);
            REQUIRE(handler.is_started());
            REQUIRE(handler.capture_history() == std::vector<LogRecord>{ any_log, any_log });

            handler.log(any_log);
            REQUIRE(handler.is_started());
            REQUIRE(handler.capture_history() == std::vector<LogRecord>{ any_log, any_log, any_log });

            handler.clear_history();
            REQUIRE(handler.is_started());
            REQUIRE(handler.capture_history().empty());
        }

        SECTION("movable")
        {
            handler.log(any_log);
            REQUIRE(handler.is_started());
            REQUIRE(handler.capture_history() == std::vector<LogRecord>{ any_log });

            auto other = std::move(handler);
            REQUIRE(not handler.is_started());
            REQUIRE(handler.capture_history().empty());
            REQUIRE(other.is_started());
            REQUIRE(other.capture_history() == std::vector<LogRecord>{ any_log });
        }
    }

    TEST_CASE("LogHandler_StdOut basics")
    {
        static constexpr LogRecord any_log{ .message = "this is a test", .level = log_level::warn };
        static const std::string expected_log_line = []
        {
            std::stringstream expected_out;
            details::log_to_stream(expected_out, any_log);
            return expected_out.str();
        }();

        std::stringstream out;
        LogHandler_StdOut handler{ out };
        REQUIRE(not handler.is_started());
        handler.start_log_handling({}, {});  // must be started to work
        REQUIRE(handler.is_started());


        SECTION("start and stop (manual)")
        {
            handler.start_log_handling({}, {});
            REQUIRE(handler.is_started());

            handler.stop_log_handling(stop_reason::manual_stop);
            REQUIRE(not handler.is_started());
        }

        handler.start_log_handling({}, {});
        REQUIRE(handler.is_started());

        SECTION("stream output")
        {
            REQUIRE(out.str().empty());

            handler.log(any_log);
            REQUIRE(handler.is_started());
            REQUIRE(out.str() == expected_log_line);

            handler.log(any_log);
            REQUIRE(handler.is_started());
            REQUIRE(out.str() == expected_log_line + expected_log_line);

            handler.log(any_log);
            REQUIRE(handler.is_started());
            REQUIRE(out.str() == expected_log_line + expected_log_line + expected_log_line);
        }

        SECTION("movable")
        {
            out.clear();
            handler.log(any_log);
            REQUIRE(handler.is_started());
            REQUIRE(out.str() == expected_log_line);

            auto other = std::move(handler);
            REQUIRE(not handler.is_started());
            REQUIRE(other.is_started());

            other.log(any_log);
            REQUIRE(not handler.is_started());
            REQUIRE(other.is_started());
            REQUIRE(out.str() == expected_log_line + expected_log_line);
        }
    }

    TEST_CASE("LogHandler_StdOut logging API basic tests")
    {
        // This generator must be kept in sync with testing::test_classic_inline_logging_api_usage()
        static constexpr auto generate_expected_output =
            [](const testing::LogHandlerTestsOptions options)
        {
            std::stringstream out;
            testing::expected_output_test_classic_inline(
                [&](LogRecord log_record) { details::log_to_stream(out, log_record); },
                options
            );
            return out.str();
        };

        static constexpr std::size_t arbitrary_log_count = 6;
        static const testing::LogHandlerTestsOptions options{ .log_count = arbitrary_log_count };
        const auto expected_output = generate_expected_output(options);

        SECTION("sunk log handler")
        {
            std::stringstream output;
            const auto results = testing::test_classic_inline_logging_api_usage(
                LogHandler_StdOut{ output },
                options
            );
            REQUIRE(results.handler.has_value());

            auto final_output = output.str();
            REQUIRE(final_output == expected_output);
        }

        SECTION("pointer to movable log handler")
        {
            std::stringstream output;
            LogHandler_StdOut handler{ output };
            const auto results = testing::test_classic_inline_logging_api_usage(&handler, options);
            REQUIRE(results.handler.has_value());
            REQUIRE(results.handler.unsafe_get<LogHandler_StdOut*>() == &handler);

            auto final_output = output.str();
            REQUIRE(final_output == expected_output);
        }
    }

    TEST_CASE("LogHandler_History logging API basic tests")
    {
        // This generator must be kept in sync with testing::test_classic_inline_logging_api_usage()
        static constexpr auto generate_expected_output =
            [](const testing::LogHandlerTestsOptions options)
        {
            std::vector<LogRecord> output;
            testing::expected_output_test_classic_inline(
                [&](LogRecord log_record) {
                    output.push_back(log_record);
            }, options);
            return output;
        };


        SECTION("sunk log handler")
        {
            const testing::LogHandlerTestsOptions options{ .log_count = 24 };
            const auto expected_output = generate_expected_output(options);

            const auto results = testing::test_classic_inline_logging_api_usage(
                LogHandler_History{ { .clear_on_stop = false } },
                options
            );
            REQUIRE(results.handler.has_value());
            REQUIRE(results.handler.unsafe_get<LogHandler_History>() != nullptr);

            const LogHandler_History& handler = *results.handler.unsafe_get<LogHandler_History>();
            const auto log_history = handler.capture_history();
            REQUIRE(not log_history.empty());
            REQUIRE(results.stats.real_output_log_count == log_history.size());
            REQUIRE(log_history == expected_output);
        }

        SECTION("pointer to movable log handler")
        {
            const testing::LogHandlerTestsOptions options{ .log_count = 69 };
            const auto expected_output = generate_expected_output(options);

            LogHandler_History handler{ { .clear_on_stop = false } };
            const auto results = testing::test_classic_inline_logging_api_usage(&handler, options);
            REQUIRE(results.handler.has_value());
            REQUIRE(results.handler.unsafe_get<LogHandler_History*>() == &handler);

            const auto log_history = handler.capture_history();
            REQUIRE(not log_history.empty());
            REQUIRE(results.stats.real_output_log_count == log_history.size());
            REQUIRE(log_history == expected_output);
        }
    }


}
