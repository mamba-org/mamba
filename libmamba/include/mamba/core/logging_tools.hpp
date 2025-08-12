// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <concepts>
#include <deque>
#include <format>
#include <print>

#include <mamba/core/logging.hpp>
#include <mamba/util/synchronized_value.hpp>

namespace mamba::logging
{
    /// `LogHandler` that retains `LogRecord`s in order of being logged.
    /// Can hold any number of records or just the specified number of last records.
    /// BEWARE: If not the max number of records is not specified, memory will be consumed at each
    /// new log record until cleared.
    class LogHandler_History  // THINK: better name
    {
        util::synchronized_value<std::deque<LogRecord>> m_log_history;
        size_t m_max_records_count = 0;

    public:

        /// Constructor specifying the maximum number of log records to keep in history.
        ///
        LogHandler_History(size_t max_records_count = 0);

        // LogHandler API

        auto start_log_handling(LoggingParams params, std::vector<log_source> sources) -> void;
        auto stop_log_handling() -> void;

        auto set_log_level(log_level new_level) -> void;
        auto set_params(LoggingParams new_params) -> void;

        auto log(logging::LogRecord record) -> void;

        auto enable_backtrace(size_t record_buffer_size) -> void;
        auto disable_backtrace() -> void;
        auto log_backtrace() noexcept -> void;
        auto log_backtrace_no_guards() noexcept -> void;

        auto flush(std::optional<log_source> source = {}) -> void;

        auto set_flush_threshold(log_level threshold_level) noexcept -> void;

        // History api
        auto capture_history() const -> std::vector<LogRecord>;
        auto clear_history();
    };

    static_assert(LogHandler<LogHandler_History>);

    /// `LogHandler` that uses `std::cout` as log record sink.
    class LogHandler_StandardOutput
    {
    public:

        // LogHandler API

        auto start_log_handling(LoggingParams params, std::vector<log_source> sources) -> void;
        auto stop_log_handling() -> void;

        auto set_log_level(log_level new_level) -> void;
        auto set_params(LoggingParams new_params) -> void;

        auto log(logging::LogRecord record) -> void;

        auto enable_backtrace(size_t record_buffer_size) -> void;
        auto disable_backtrace() -> void;
        auto log_backtrace() noexcept -> void;
        auto log_backtrace_no_guards() noexcept -> void;

        auto flush(std::optional<log_source> source = {}) -> void;

        auto set_flush_threshold(log_level threshold_level) noexcept -> void;
    };

    static_assert(LogHandler<LogHandler_StandardOutput>);

    /// `LogHandler` that uses `std::print` as log record sink.
    class LogHandler_StandardPrint
    {
    public:

        // LogHandler API

        auto start_log_handling(LoggingParams params, std::vector<log_source> sources) -> void;
        auto stop_log_handling() -> void;

        auto set_log_level(log_level new_level) -> void;
        auto set_params(LoggingParams new_params) -> void;

        auto log(logging::LogRecord record) -> void;

        auto enable_backtrace(size_t record_buffer_size) -> void;
        auto disable_backtrace() -> void;
        auto log_backtrace() noexcept -> void;
        auto log_backtrace_no_guards() noexcept -> void;

        auto flush(std::optional<log_source> source = {}) -> void;

        auto set_flush_threshold(log_level threshold_level) noexcept -> void;
    };

    static_assert(LogHandler<LogHandler_StandardPrint>);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////


    auto
    LogHandler_StandardOutput::start_log_handling(LoggingParams, std::vector<log_source>)
        -> void
    {
        // nothing to do
    }

    auto LogHandler_StandardOutput::stop_log_handling() -> void
    {
        // nothing to do
    }

    auto LogHandler_StandardOutput::set_log_level(log_level new_level) -> void
    {
    }

    auto LogHandler_StandardOutput::set_params(LoggingParams new_params) -> void
    {
    }

    auto LogHandler_StandardOutput::log(logging::LogRecord record) -> void
    {
    }

    auto LogHandler_StandardOutput::enable_backtrace(size_t record_buffer_size) -> void
    {
    }

    auto LogHandler_StandardOutput::disable_backtrace() -> void
    {
    }

    auto LogHandler_StandardOutput::log_backtrace() noexcept -> void
    {
    }

    auto LogHandler_StandardOutput::log_backtrace_no_guards() noexcept -> void
    {
    }

    auto LogHandler_StandardOutput::flush(std::optional<log_source> source = {}) -> void
    {
    }

    auto LogHandler_StandardOutput::set_flush_threshold(log_level threshold_level) noexcept -> void
    {
    }

}