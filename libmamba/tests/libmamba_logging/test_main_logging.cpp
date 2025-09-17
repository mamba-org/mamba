#define CATCH_CONFIG_MAIN

#include <catch2/catch_all.hpp>

#include <mamba/core/logging.hpp>

#include "test_logging_common.hpp"

namespace mamba::logging
{
    //namespace
    //{
    //    struct LogHandlerTestsResult
    //    {
    //        testing::Stats stats;
    //        AnyLogHandler handler;
    //    };

    //}

    //template <LogHandlerOrPtr T>
    //auto test_classic_inline_logging_api_usage(T&& handler, size_t log_count) -> LogHandlerTestsResult
    //{
    //    testing::Stats stats;

    //    auto previous_handler = set_log_handler(std::forward<T>(handler));
    //    REQUIRE(not previous_handler.has_value());

    //    SECTION("start stop(manual) start")
    //    {
    //        auto original_handler = stop_logging(stop_reason::manual_stop);
    //        REQUIRE(original_handler.has_value());

    //        auto previous_handler = set_log_handler(std::move(original_handler));
    //        REQUIRE(not previous_handler.has_value());
    //    }

    //    SECTION("change parameters")
    //    {
    //        set_log_level(log_level::debug);
    //        REQUIRE(get_log_level() == log_level::debug);

    //        set_log_level(log_level::warn);
    //        REQUIRE(get_log_level() == log_level::warn);

    //        // const LoggingParams logging_params{ };
    //        // set_logging_params()
    //    }


    //    return { .stats = std::move(stats), .handler = stop_logging(stop_reason::program_exit) };
    //}

    //TEST_CASE("logging API basic tests")
    //{
    //    SECTION("sunk log handler")
    //    {
    //        const auto results = test_classic_inline_logging_api_usage(testing::LogHandler_Tester{}, 42);
    //        REQUIRE(results.handler.has_value());
    //        REQUIRE(
    //            results.stats
    //            == results.handler.unsafe_get<testing::LogHandler_Tester>()->capture_stats()
    //        );
    //    }

    //    SECTION("pointer to movable log handler")
    //    {
    //        testing::LogHandler_Tester tester;
    //        const auto results = test_classic_inline_logging_api_usage(&tester, 96);
    //        REQUIRE(results.handler.has_value());
    //        REQUIRE(results.handler.unsafe_get<testing::LogHandler_Tester>() == &tester);
    //        REQUIRE(results.stats == tester.capture_stats());
    //    }

    //    SECTION("supports non-movable log handlers")
    //    {
    //        testing::LogHandler_NotMovable not_movable;
    //        const auto results = test_classic_inline_logging_api_usage(&not_movable, 1234);
    //    }
    //}

}
