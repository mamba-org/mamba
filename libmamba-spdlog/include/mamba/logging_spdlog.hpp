// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_LOGGING_SPDLOG_HPP
#define MAMBA_LOGGING_SPDLOG_HPP

#include <memory>
#include <vector>

#include <spdlog/common.h>

#include <mamba/core/logging.hpp>

namespace mamba::logging::spdlogimpl
{

    /** @returns The provided `log_level` value converted to the equivalent value for `spdlog`. */
    constexpr auto to_spdlog(log_level level) -> spdlog::level::level_enum
    {
        static_assert(sizeof(log_level) == sizeof(spdlog::level::level_enum));
        static_assert(
            static_cast<int>(log_level::all) == static_cast<int>(spdlog::level::level_enum::n_levels)
        );
        return static_cast<spdlog::level::level_enum>(level);
    }

    struct LogHandler_spdlog_Options
    {
        /** At each call to `start_log_handling`, after having setup the internal loggers,
            we remove the sinks and replace them by a null sink.

            Mostly useful in tests.s
        */
        bool redirect_to_null_sink = false;
    };

    /** `LogHandler` implementation using `spdlog` library.

        Essentially translates the calls to the interface specified by `mamba::logging::LogHandler`
        into calls to `spdlog`'s API.
        This implementation doesn't keep data, every logger implementation is owned by
        the `spdlog` library.

        @see `mamba::logging::LogHandler`
    */
    class LogHandler_spdlog
    {
    public:

        LogHandler_spdlog(LogHandler_spdlog_Options options = LogHandler_spdlog_Options{});
        ~LogHandler_spdlog();

        LogHandler_spdlog(const LogHandler_spdlog& other) = delete;
        LogHandler_spdlog& operator=(const LogHandler_spdlog& other) = delete;

        LogHandler_spdlog(LogHandler_spdlog&& other) noexcept;
        LogHandler_spdlog& operator=(LogHandler_spdlog&& other) noexcept;

        /** `LogHandler` API implementation, @see mamba::logging::LogHandler for the expected
           behavior.

            All these functions are thread-safe except for `start_log_handling` and
           `stop_log_handling`.

            pre-conditions:
                - `is_started() == true`, except for `start_log_handling` and `stop_log_handling`
                  which don't require this pre-condition.

            post-conditions:
                - after `start_log_handling` call:`is_started() == true`;
                - after `stop_log_handling` call: `is_started() == true`.
        */
        ///@{
        auto start_log_handling(LoggingParams params, std::vector<log_source> sources) -> void;
        auto stop_log_handling(stop_reason reason) -> void;

        auto set_log_level(log_level new_level) -> void;
        auto set_params(LoggingParams new_params) -> void;

        auto log(logging::LogRecord record) -> void;

        auto enable_backtrace(size_t record_buffer_size) -> void;
        auto disable_backtrace() -> void;
        auto log_backtrace() -> void;
        auto log_backtrace_no_guards() -> void;

        auto flush(std::optional<log_source> source = {}) -> void;

        auto set_flush_threshold(log_level threshold_level) -> void;
        ///@}

        /** @returns `true` after `start_log_handling` has been called and `stop_log_handling` was
            not called since.
        */
        auto is_started() const -> bool;

        /** After this call, all log records will be routed to the null sink,
            which implies that all log records will be ignored.
        */
        auto redirect_all_to_null_sink() -> void;

    private:

        struct Impl;
        std::unique_ptr<Impl> pimpl;
    };

    static_assert(logging::LogHandler<LogHandler_spdlog>);

}

#include "./logging_spdlog_impl.hpp"

#endif
