// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <memory>
#include <typeindex>

#include <catch2/catch_all.hpp>

#include <mamba/core/logging.hpp>
#include <mamba/util/synchronized_value.hpp>

#include "test_logging_common.hpp"

namespace mamba::logging
{
    namespace testing
    {

        template <class T>
        concept CanGoIntoAnyLogHandler = requires(T x, AnyLogHandler& handler) {
            AnyLogHandler{ std::move(x) };
            AnyLogHandler(std::move(x));
            handler = std::move(x);
        };

        static_assert(not CanGoIntoAnyLogHandler<NotALogHandler>);
        static_assert(not CanGoIntoAnyLogHandler<NotALogHandler*>);

        static_assert(CanGoIntoAnyLogHandler<LogHandler_Tester>);
        static_assert(CanGoIntoAnyLogHandler<LogHandler_Tester*>);

        static_assert(not CanGoIntoAnyLogHandler<LogHandler_NotMovable>);
        static_assert(CanGoIntoAnyLogHandler<LogHandler_NotMovable*>);
    }

    TEST_CASE("AnyLogHandler basics")
    {
        SECTION("empty by default")
        {
            AnyLogHandler x;
            REQUIRE(not x.has_value());
            REQUIRE(static_cast<bool>(x) == false);
            REQUIRE(not x.type_id().has_value());
        }


        SECTION("access to implementation")
        {
            SECTION("empty")
            {
                AnyLogHandler x;
                REQUIRE(x.unsafe_get<testing::LogHandler_Tester>() == nullptr);
                REQUIRE(std::as_const(x).unsafe_get<testing::LogHandler_Tester>() == nullptr);
            }

            SECTION("with sunk handler")
            {
                AnyLogHandler x{ testing::LogHandler_Tester{} };
                REQUIRE(x.unsafe_get<testing::LogHandler_Tester>() != nullptr);
                REQUIRE(std::as_const(x).unsafe_get<testing::LogHandler_Tester>() != nullptr);
            }

            SECTION("with pointer to handler")
            {
                testing::LogHandler_NotMovable handler;
                AnyLogHandler x{ &handler };
                REQUIRE(x.unsafe_get<testing::LogHandler_NotMovable*>() == &handler);
                REQUIRE(std::as_const(x).unsafe_get<testing::LogHandler_NotMovable*>() == &handler);
            }
        }

        SECTION("movable")
        {
            testing::LogHandler_Tester handler;
            const auto* impl_ptr = handler.pimpl.get();
            REQUIRE(impl_ptr);

            AnyLogHandler x{ std::move(handler) };
            REQUIRE(handler.pimpl.get() == nullptr);
            REQUIRE(x.has_value());
            REQUIRE(static_cast<bool>(x) == true);
            REQUIRE(x.type_id().has_value());
            REQUIRE(x.type_id().value() == typeid(handler));
            REQUIRE(x.unsafe_get<testing::LogHandler_Tester>()->pimpl.get() == impl_ptr);

            AnyLogHandler y = std::move(x);
            REQUIRE(y.has_value());
            REQUIRE(static_cast<bool>(y) == true);
            REQUIRE(y.type_id().has_value());
            REQUIRE(y.type_id().value() == typeid(handler));
            REQUIRE(y.unsafe_get<testing::LogHandler_Tester>()->pimpl.get() == impl_ptr);
            REQUIRE(not x.has_value());
            REQUIRE(not static_cast<bool>(x) == true);
            REQUIRE(not x.type_id().has_value());
            REQUIRE(x.unsafe_get<testing::LogHandler_Tester>() == nullptr);

            AnyLogHandler z;
            z = std::move(y);
            REQUIRE(z.has_value());
            REQUIRE(static_cast<bool>(z) == true);
            REQUIRE(z.type_id().has_value());
            REQUIRE(z.type_id().value() == typeid(handler));
            REQUIRE(z.unsafe_get<testing::LogHandler_Tester>()->pimpl.get() == impl_ptr);
            REQUIRE(not y.has_value());
            REQUIRE(not static_cast<bool>(y) == true);
            REQUIRE(not y.type_id().has_value());
            REQUIRE(x.unsafe_get<testing::LogHandler_Tester>() == nullptr);
        }

        SECTION("pointer to non-movable LogHandler")
        {
            testing::LogHandler_NotMovable handler;
            AnyLogHandler x{ &handler };
            REQUIRE(x.has_value());
            REQUIRE(static_cast<bool>(x) == true);
            REQUIRE(x.type_id().has_value());
            REQUIRE(x.type_id().value() == typeid(&handler));
            REQUIRE(x.unsafe_get<testing::LogHandler_NotMovable*>() == &handler);
        }

    }

    TEST_CASE("AnyLogHandler handler ownership")
    {
        using Stats = testing::Stats;

        SECTION("owns moved-in/sunk value")
        {
            testing::LogHandler_Tester handler;
            const auto* impl_ptr = handler.pimpl.get();
            REQUIRE(impl_ptr);

            AnyLogHandler x{ std::move(handler) };
            REQUIRE(handler.pimpl.get() == nullptr);
            REQUIRE(x.has_value());
            REQUIRE(static_cast<bool>(x) == true);
            REQUIRE(x.type_id().has_value());
            REQUIRE(x.type_id().value() == typeid(handler));
            REQUIRE(impl_ptr->stats.value() == Stats{});

            x.start_log_handling({}, {});
            REQUIRE(x.has_value());
            REQUIRE(static_cast<bool>(x) == true);
            REQUIRE(x.type_id().has_value());
            REQUIRE(x.type_id().value() == typeid(handler));
            REQUIRE(impl_ptr->stats.value() == Stats{ .start_count = 1 });

            AnyLogHandler y = std::move(x);
            REQUIRE(y.has_value());
            REQUIRE(static_cast<bool>(y) == true);
            REQUIRE(y.type_id().has_value());
            REQUIRE(y.type_id().value() == typeid(handler));
            REQUIRE(not x.has_value());
            REQUIRE(not static_cast<bool>(x) == true);
            REQUIRE(not x.type_id().has_value());
            REQUIRE(impl_ptr->stats.value() == Stats{ .start_count = 1 });

            y.stop_log_handling();
            REQUIRE(y.has_value());
            REQUIRE(static_cast<bool>(y) == true);
            REQUIRE(y.type_id().has_value());
            REQUIRE(y.type_id().value() == typeid(handler));
            REQUIRE(not x.has_value());
            REQUIRE(not static_cast<bool>(x) == true);
            REQUIRE(not x.type_id().has_value());
            REQUIRE(impl_ptr->stats.value() == Stats{ .start_count = 1, .stop_count = 1 });
        }

        SECTION("does not own pointed handler")
        {
            testing::LogHandler_Tester handler;
            const auto* original_impl_ptr = handler.pimpl.get();
            REQUIRE(original_impl_ptr);

            AnyLogHandler x{ &handler };
            REQUIRE(handler.pimpl.get() != nullptr);
            REQUIRE(x.has_value());
            REQUIRE(static_cast<bool>(x) == true);
            REQUIRE(x.type_id().has_value());
            REQUIRE(x.type_id().value() == typeid(&handler));
            REQUIRE(handler.pimpl->stats.value() == Stats{});
            REQUIRE(original_impl_ptr == handler.pimpl.get());

            x.start_log_handling({}, {});
            REQUIRE(x.has_value());
            REQUIRE(static_cast<bool>(x) == true);
            REQUIRE(x.type_id().has_value());
            REQUIRE(x.type_id().value() == typeid(&handler));
            REQUIRE(handler.pimpl->stats.value() == Stats{ .start_count = 1 });

            AnyLogHandler y = std::move(x);
            REQUIRE(y.has_value());
            REQUIRE(static_cast<bool>(y) == true);
            REQUIRE(y.type_id().has_value());
            REQUIRE(y.type_id().value() == typeid(&handler));
            REQUIRE(not x.has_value());
            REQUIRE(not static_cast<bool>(x) == true);
            REQUIRE(not x.type_id().has_value());
            REQUIRE(handler.pimpl->stats.value() == Stats{ .start_count = 1 });
            REQUIRE(original_impl_ptr == handler.pimpl.get());

            y.stop_log_handling();
            REQUIRE(y.has_value());
            REQUIRE(static_cast<bool>(y) == true);
            REQUIRE(y.type_id().has_value());
            REQUIRE(y.type_id().value() == typeid(&handler));
            REQUIRE(not x.has_value());
            REQUIRE(not static_cast<bool>(x) == true);
            REQUIRE(not x.type_id().has_value());
            REQUIRE(handler.pimpl->stats.value() == Stats{ .start_count = 1, .stop_count = 1 });
            REQUIRE(original_impl_ptr == handler.pimpl.get());
        }
    }

    TEST_CASE("AnyLogHandler LogHandler operations")
    {
        using Stats = testing::Stats;

        testing::LogHandler_Tester handler;
        auto& stats = handler.pimpl->stats;

        AnyLogHandler x{ std::move(handler) };
        REQUIRE(stats == Stats{});

        // All `LogHandler` operations should be forwarded to the implementation.

        x.start_log_handling({ .logging_level = log_level::trace }, all_log_sources());
        REQUIRE(
            stats == Stats{ .start_count = 1, .current_params = { .logging_level = log_level::trace } }
        );

        x.stop_log_handling();
        REQUIRE(
            stats
            == Stats{ .start_count = 1,
                      .stop_count = 1,
                      .current_params = { .logging_level = log_level::trace } }
        );

        x.start_log_handling({}, all_log_sources());
        REQUIRE(stats == Stats{ .start_count = 2, .stop_count = 1 });

        x.set_log_level(log_level::critical);  // default log level
        REQUIRE(
            stats
            == Stats{ .start_count = 2,
                      .stop_count = 1,
                      .log_level_change_count = 1,
                      .current_params = { .logging_level = log_level::critical } }
        );

        x.set_params(LoggingParams{});
        REQUIRE(
            stats
            == Stats{ .start_count = 2,
                      .stop_count = 1,
                      .log_level_change_count = 1,
                      .params_change_count = 1 }
        );

        static constexpr size_t arbitrary_log_count = 42;
        for (size_t log_idx = 0; log_idx < arbitrary_log_count; ++log_idx)
        {
            x.log(LogRecord{});
            REQUIRE(
                stats
                == Stats{ .start_count = 2,
                          .stop_count = 1,
                          .log_count = log_idx + 1,
                          .log_level_change_count = 1,
                          .params_change_count = 1 }
            );
        }

        static constexpr size_t arbitrary_backtrace_size = 1234;
        x.enable_backtrace(arbitrary_backtrace_size);
        REQUIRE(
            stats
            == Stats{ .start_count = 2,
                      .stop_count = 1,
                      .log_count = arbitrary_log_count,
                      .log_level_change_count = 1,
                      .params_change_count = 1,
                      .backtrace_enabled_count = 1,
                      .backtrace_size = arbitrary_backtrace_size

            }
        );

        x.log_backtrace();
        REQUIRE(
            stats
            == Stats{ .start_count = 2,
                      .stop_count = 1,
                      .log_count = arbitrary_log_count,
                      .log_level_change_count = 1,
                      .params_change_count = 1,
                      .backtrace_enabled_count = 1,
                      .backtrace_log_count = 1,
                      .backtrace_size = arbitrary_backtrace_size

            }
        );

        x.log_backtrace_no_guards();
        REQUIRE(
            stats
            == Stats{ .start_count = 2,
                      .stop_count = 1,
                      .log_count = arbitrary_log_count,
                      .log_level_change_count = 1,
                      .params_change_count = 1,
                      .backtrace_enabled_count = 1,
                      .backtrace_log_count = 1,
                      .backtrace_log_no_guard_count = 1,
                      .backtrace_size = arbitrary_backtrace_size }
        );

        x.flush();
        REQUIRE(
            stats
            == Stats{ .start_count = 2,
                      .stop_count = 1,
                      .log_count = arbitrary_log_count,
                      .log_level_change_count = 1,
                      .params_change_count = 1,
                      .backtrace_enabled_count = 1,
                      .backtrace_log_count = 1,
                      .backtrace_log_no_guard_count = 1,
                      .flush_all_count = 1,
                      .backtrace_size = arbitrary_backtrace_size }
        );

        x.flush(log_source::tests);
        REQUIRE(
            stats
            == Stats{ .start_count = 2,
                      .stop_count = 1,
                      .log_count = arbitrary_log_count,
                      .log_level_change_count = 1,
                      .params_change_count = 1,
                      .backtrace_enabled_count = 1,
                      .backtrace_log_count = 1,
                      .backtrace_log_no_guard_count = 1,
                      .flush_all_count = 1,
                      .flush_specific_source_count = 1,
                      .backtrace_size = arbitrary_backtrace_size }
        );

        x.set_flush_threshold(log_level::all);
        REQUIRE(
            stats
            == Stats{ .start_count = 2,
                      .stop_count = 1,
                      .log_count = arbitrary_log_count,
                      .log_level_change_count = 1,
                      .params_change_count = 1,
                      .backtrace_enabled_count = 1,
                      .backtrace_log_count = 1,
                      .backtrace_log_no_guard_count = 1,
                      .flush_all_count = 1,
                      .flush_specific_source_count = 1,
                      .flush_threshold_change_count = 1,
                      .backtrace_size = arbitrary_backtrace_size,
                      .flush_threshold = log_level::all }
        );
    }
}