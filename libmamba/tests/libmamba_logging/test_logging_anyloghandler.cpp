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

namespace mamba::logging
{
    namespace testing
    {
        struct NotALogHandler
        {
        };

        static_assert(not LogHandler<NotALogHandler>);

        template <class T>
        concept CanGoIntoAnyLogHandler = requires(T x, AnyLogHandler& handler) {
            AnyLogHandler{ std::move(x) };
            AnyLogHandler(std::move(x));
            handler = std::move(x);
        };

        static_assert(not CanGoIntoAnyLogHandler<NotALogHandler>);
        static_assert(not CanGoIntoAnyLogHandler<NotALogHandler*>);

        struct LogHandler_Tester
        {
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

                LoggingParams current_params;
                std::size_t backtrace_size = 0;
                log_level flush_threshold = log_level::off;

                auto operator==(const Stats& other) const noexcept -> bool = default;
            };

            struct Impl
            {
                util::synchronized_value<Stats> stats;
            };

            std::unique_ptr<Impl> pimpl = std::make_unique<Impl>();

            LogHandler_Tester() = default;

            LogHandler_Tester(LogHandler_Tester&&) noexcept = default;
            LogHandler_Tester(const LogHandler_Tester&) = default;
            LogHandler_Tester& operator=(LogHandler_Tester&&) noexcept = default;
            LogHandler_Tester& operator=(const LogHandler_Tester&) = default;

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
        static_assert(CanGoIntoAnyLogHandler<LogHandler_Tester>);
        static_assert(CanGoIntoAnyLogHandler<LogHandler_Tester*>);

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

            AnyLogHandler y = std::move(x);
            REQUIRE(y.has_value());
            REQUIRE(static_cast<bool>(y) == true);
            REQUIRE(y.type_id().has_value());
            REQUIRE(y.type_id().value() == typeid(handler));
            REQUIRE(not x.has_value());
            REQUIRE(not static_cast<bool>(x) == true);
            REQUIRE(not x.type_id().has_value());

            AnyLogHandler z;
            z = std::move(y);
            REQUIRE(z.has_value());
            REQUIRE(static_cast<bool>(z) == true);
            REQUIRE(z.type_id().has_value());
            REQUIRE(z.type_id().value() == typeid(handler));
            REQUIRE(not y.has_value());
            REQUIRE(not static_cast<bool>(y) == true);
            REQUIRE(not y.type_id().has_value());
        }

        SECTION("pointer to non-movable LogHandler")
        {
            testing::LogHandler_NotMovable handler;
            AnyLogHandler x{ &handler };
            REQUIRE(x.has_value());
            REQUIRE(static_cast<bool>(x) == true);
            REQUIRE(x.type_id().has_value());
            REQUIRE(x.type_id().value() == typeid(&handler));
        }
    }

    TEST_CASE("AnyLogHandler handler ownership")
    {
        using Stats = testing::LogHandler_Tester::Stats;

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
        using Stats = testing::LogHandler_Tester::Stats;

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