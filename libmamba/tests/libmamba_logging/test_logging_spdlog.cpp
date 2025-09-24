// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <deque>

#include <catch2/catch_all.hpp>
#include <fmt/core.h>

#include <mamba/core/logging_spdlog.hpp>
#include <mamba/core/util_scope.hpp>

#include "test_logging_common.hpp"

namespace mamba::logging
{

    TEST_CASE("LogHandler_spdlog basics")
    {
        static constexpr LogRecord any_log{ .message = "this is a test", .level = log_level::warn, .source = log_source::tests };

        spdlogimpl::LogHandler_spdlog handler;

        // we need this handler to cleanup loggers properly at the end of this test
        on_scope_exit _{ [&] { handler.stop_log_handling(stop_reason::manual_stop); } };

        REQUIRE(not handler.is_started());
        handler.start_log_handling({}, testing::testing_log_sources());
        REQUIRE(handler.is_started());


        // start and stop (manual)
        {
            handler.start_log_handling({}, testing::testing_log_sources());
            REQUIRE(handler.is_started());

            handler.stop_log_handling(stop_reason::manual_stop);
            REQUIRE(not handler.is_started());
        }

        handler.start_log_handling({}, testing::testing_log_sources());
        REQUIRE(handler.is_started());

        // movable
        {
            handler.log(any_log);
            REQUIRE(handler.is_started());

            auto other = std::move(handler);
            REQUIRE(not handler.is_started());
            REQUIRE(other.is_started());
        }
    }

    TEST_CASE("LogHandler_spdlog logging API basic tests")
    {
        static constexpr std::size_t arbitrary_log_count = 123;
        static const testing::LogHandlerTestsOptions options{
            .log_count = arbitrary_log_count,

            // spdlog's log handler only cleanup explicitly when
            // the stop is manual, otherwise it assumes spdlog will
            // do the proper cleanup at program exit.
            // Because we are in tests we need the cleanups between
            // each test run, so we want all stops to be manual.
            .last_stop_reason = stop_reason::manual_stop
        };

        SECTION("sunk log handler")
        {
            std::stringstream output;
            const auto results = testing::test_classic_inline_logging_api_usage(
                spdlogimpl::LogHandler_spdlog{},
                options
            );
            REQUIRE(results.handler.has_value());
            // TODO: find a way to check the resulting output
        }

        SECTION("pointer to movable log handler")
        {
            std::stringstream output;
            spdlogimpl::LogHandler_spdlog handler;
            const auto results = testing::test_classic_inline_logging_api_usage(&handler, options);
            REQUIRE(results.handler.has_value());
            REQUIRE(results.handler.unsafe_get<spdlogimpl::LogHandler_spdlog*>() == &handler);
            // TODO: find a way to check the resulting output
        }
    }


}
