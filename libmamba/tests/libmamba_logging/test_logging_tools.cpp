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

// TODO:
// - logging system functions checks
// - AnyLogHandler checks
//
// for each log-handler type
// - test details functions?
// - at least movable and keep state correct
// - test all logging operations
// - test additional functionalities specific to log handler type
// - stress test when logging (and other functions?) from multiple threads
//

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

}
