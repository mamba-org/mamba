#include <catch2/catch_session.hpp>

int
main(int argc, char* argv[])
{
    Catch::Session session;

    // Set default test order to declaration order (pre-3.9 behavior)
    // This overrides Catch2 3.9's default random order
    session.configData().runOrder = Catch::TestRunOrder::Declared;

    // Apply command line arguments (which may override the default)
    int returnCode = session.applyCommandLine(argc, argv);
    if (returnCode != 0)
    {
        return returnCode;
    }

    // Run tests
    return session.run();
}

#include <mamba/core/logging.hpp>
#include <mamba/testing/test_logging_common.hpp>

namespace mamba::logging
{
    TEST_CASE("logging API basic tests")
    {
        // sunk log handler
        {
            const auto results = testing::test_classic_inline_logging_api_usage(
                testing::LogHandler_Tester{},
                { .log_count = 42 }
            );
            REQUIRE(results.handler.has_value());
            REQUIRE(
                results.stats
                == results.handler.unsafe_get<testing::LogHandler_Tester>()->capture_stats()
            );
        }

        // pointer to movable log handler
        {
            testing::LogHandler_Tester tester;
            const auto results = testing::test_classic_inline_logging_api_usage(
                &tester,
                { .log_count = 96 }
            );
            REQUIRE(results.handler.has_value());
            REQUIRE(results.handler.unsafe_get<testing::LogHandler_Tester*>() == &tester);
            REQUIRE(results.stats == tester.capture_stats());
        }

        // supports pointer to non-movable log handlers
        {
            testing::LogHandler_NotMovable not_movable;
            const auto results = testing::test_classic_inline_logging_api_usage(
                &not_movable,
                { .log_count = 1234 }
            );
        }
    }

}
