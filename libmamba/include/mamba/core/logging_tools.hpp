// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <atomic>
#include <concepts>
#include <deque>
#include <iostream>
#include <optional>

#include <fmt/core.h>  // TODO: replace by `<format>` once available on all ci compilers

#include <mamba/core/logging.hpp>
#include <mamba/util/synchronized_value.hpp>

namespace mamba::logging
{

    namespace details
    {
        inline auto queue_push(std::deque<LogRecord>& queue, size_t max_elements, LogRecord record)
            -> void
        {
            queue.push_back(std::move(record));
            if (max_elements > 0 and queue.size() > max_elements)
            {
                queue.pop_front();
            }
        }

        /** Backtrace feature implementation in it's most basic form.

            This is the simplest implementation for a backtrace feature
            as described by @see `mamba::logging::enable_backtrace`.

            Mainly used in `mamba::logging::LogHandler` basic implementations.

        */
        class BasicBacktrace
        {
            std::deque<LogRecord> backtrace;
            size_t backtrace_max = 0;  // 0 means disabled

        public:

            /** @returns `true` if the backtrace feature is enabled, `false` otherwise. */
            auto is_enabled() const
            {
                return backtrace_max > 0;
            }

            /** If the backtrace feature is enabled, moves the log record into the backtrace
                history and returns `true`. Otherwise do nothing and returns `false`.

                The log record is taken by reference to allow taking ownership of it through
                a move, but also not taking ownership and not forcing a copy if we actually
                don't need it.
            */
            auto push_if_enabled(LogRecord& record) -> bool
            {
                if (not is_enabled())
                {
                    return false;
                }

                queue_push(backtrace, backtrace_max, std::move(record));

                return true;
            }

            /** Changes the number of log records kept in the backtrace history.

                If set to zero, the feature is disabled.
            */
            auto set_max_trace(size_t max_trace_size) -> void
            {
                backtrace_max = max_trace_size;
                if (backtrace_max > 0)
                {
                    while (backtrace.size() > backtrace_max)
                    {
                        backtrace.pop_front();
                    }
                }
                else
                {
                    backtrace.clear();
                }
            }

            auto disable() -> void
            {
                set_max_trace(0);
            }

            auto clear() -> void
            {
                backtrace.clear();
            }

            auto begin()
            {
                return backtrace.begin();
            }

            auto begin() const
            {
                return backtrace.begin();
            }

            auto end()
            {
                return backtrace.end();
            }

            auto end() const
            {
                return backtrace.end();
            }

            auto size() const -> std::size_t
            {
                return backtrace.size();
            }

            auto empty() const -> bool
            {
                return backtrace.empty();
            }
        };

        inline auto as_log(const std::source_location& location) -> std::string
        {
            return fmt::format(
                "{}:{}:{} {}",
                location.file_name(),
                location.line(),
                location.column(),
                location.function_name()
            );
        }

        struct log_to_stream_options
        {
            bool with_location = false;
        };

        inline auto
        log_to_stream(std::ostream& out, const LogRecord& record, log_to_stream_options options = {})
            -> std::ostream&
        {
            auto location_str = options.with_location
                                    ? fmt::format(" ({})", details::as_log(record.location))
                                    : std::string{};

            out << fmt::format(
                "\n{} {}{} : {}",
                name_of(record.level),
                name_of(record.source),
                location_str,
                record.message
            );

            return out;
        }
    }

    struct LogHandler_History_Options  // not nested type because clang and gcc dont like it
    {
        size_t max_records_count = 0;
        bool clear_on_stop = true;
    };

    /** `LogHandler` that retains `LogRecord`s in order of being logged.
        Can hold any number of records or just the specified number of last records.
        BEWARE: If the max number of records is not specified, memory will be consumed at each
        new log record until cleared.

        All operations are thread-safe except move operations.
    */
    class LogHandler_History
    {
    public:

        using Options = LogHandler_History_Options;

        /** Constructor specifying the maximum number of log records to keep in history.

            post-condition: `is_started() == false` until `start_log_handler` is called.
        */
        LogHandler_History(Options options = Options{});

        // Only allow moves, not thread-safe.

        LogHandler_History(const LogHandler_History& other) = delete;
        LogHandler_History& operator=(const LogHandler_History& other) = delete;

        LogHandler_History(LogHandler_History&& other) noexcept = default;
        LogHandler_History& operator=(LogHandler_History&& other) noexcept = default;

        // LogHandler API - thread-safe

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
        auto start_log_handling(LoggingParams params, const std::vector<log_source>&) -> void;
        auto stop_log_handling(stop_reason reason) -> void;

        auto set_log_level(log_level new_level) -> void;
        auto set_params(LoggingParams new_params) -> void;

        auto log(LogRecord record) -> void;

        auto enable_backtrace(size_t record_buffer_size) -> void;
        auto disable_backtrace() -> void;
        auto log_backtrace() -> void;
        auto log_backtrace_no_guards() -> void;

        auto flush(std::optional<log_source> source = {}) -> void;

        auto set_flush_threshold(log_level threshold_level) -> void;
        ///@}

        ////////////////////////////////////////////
        // History api - thread-safe

        /** @returns A copy of the current log record history.
                     The value should be considered immediately obsolete
                     as new log records could be pushed concurrently.
                     The returned history will be empty if `is_started()` == false.
        */
        auto capture_history() const -> std::vector<LogRecord>;

        /** Clears the internal history.

            post-condition: `capture_history().empty() == true`.
        */
        auto clear_history();

        /** @returns `true` if `start_log_handling` has been called and since that
            call `stop_log_handling` has not been called yet, `false` otherwise.
        */
        auto is_started() const -> bool;

        /** @returns The options this log handler has been constructed with. */
        auto get_options() const -> const Options&
        {
            return options;
        }

    private:

        struct Data
        {
            std::deque<LogRecord> history;
            details::BasicBacktrace backtrace;
        };

        struct Impl
        {
            util::synchronized_value<Data> data;
            std::atomic<log_level> current_log_level = log_level::info;
        };

        std::unique_ptr<Impl> pimpl;
        Options options;
    };

    static_assert(LogHandler<LogHandler_History>);

    struct LogHandler_StdOut_Options  // not nested type because clang and gcc dont like it
    {
        bool clear_on_stop = true;
    };

    /** `LogHandler` that uses `std::ostream` as log record sink, set to `std::out` by default.
    */
    class LogHandler_StdOut
    {
    public:

        using Options = LogHandler_StdOut_Options;

        /** Constructor providing the output stream to write logs into, `std::cout` by default.

            Ownership of the provided output stream is not taken.
            The lifetime of the provided output stream object must be greater than the lifetime
            of this object.

            post-condition: `is_started() == false` until `start_log_handler` is called.
        */
        LogHandler_StdOut(std::ostream& out_ = std::cout, Options options = Options{});

        LogHandler_StdOut(const LogHandler_StdOut& other) = delete;
        LogHandler_StdOut& operator=(const LogHandler_StdOut& other) = delete;

        LogHandler_StdOut(LogHandler_StdOut&& other) noexcept;
        LogHandler_StdOut& operator=(LogHandler_StdOut&& other) noexcept;

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
        auto start_log_handling(LoggingParams params, const std::vector<log_source>&) -> void;
        auto stop_log_handling(stop_reason reason) -> void;

        auto set_log_level(log_level new_level) -> void;
        auto set_params(LoggingParams new_params) -> void;

        auto log(LogRecord record) -> void;

        auto enable_backtrace(size_t record_buffer_size) -> void;
        auto disable_backtrace() -> void;
        auto log_backtrace() -> void;
        auto log_backtrace_no_guards() -> void;

        auto flush(std::optional<log_source> source = {}) -> void;

        auto set_flush_threshold(log_level threshold_level) -> void;
        ///@}

        // Additional functionalities


        /** @returns `true` if `start_log_handling` has been called and since that
            call `stop_log_handling` has not been called yet, `false` otherwise.
        */
        auto is_started() const -> bool;

        /** @returns The options this log handler has been constructed with.
         */
        auto get_options() const -> const Options&
        {
            return options;
        }

    private:

        struct Impl
        {
            util::synchronized_value<details::BasicBacktrace> backtrace;
            std::atomic<log_level> current_log_level = log_level::warn;
            std::atomic<bool> log_location = false;
            std::atomic<log_level> flush_threshold = log_level::warn;
        };

        std::ostream* out;
        std::unique_ptr<Impl> pimpl;
        Options options;
    };

    static_assert(LogHandler<LogHandler_StdOut>);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    inline LogHandler_History::LogHandler_History(Options options_)
        : options(std::move(options_))
    {
    }

    inline auto
    LogHandler_History::start_log_handling(LoggingParams params, const std::vector<log_source>&)
        -> void
    {
        if (not pimpl)
        {
            pimpl = std::make_unique<Impl>();
        }

        pimpl->current_log_level = params.logging_level;
        pimpl->data.unsafe_get().backtrace.set_max_trace(params.log_backtrace);
    }

    inline auto LogHandler_History::stop_log_handling(stop_reason) -> void
    {
        if (options.clear_on_stop)
        {
            pimpl.reset();
        }
    }

    inline auto LogHandler_History::set_log_level(log_level new_level) -> void
    {
        assert(pimpl);
        pimpl->current_log_level = new_level;
    }

    inline auto LogHandler_History::set_params(LoggingParams new_params) -> void
    {
        assert(pimpl);
        pimpl->current_log_level = new_params.logging_level;
        pimpl->data->backtrace.set_max_trace(new_params.log_backtrace);
    }

    inline auto LogHandler_History::log(LogRecord record) -> void
    {
        assert(pimpl);
        if (pimpl->current_log_level < record.level)
        {
            return;
        }

        auto synched_data = pimpl->data.synchronize();
        if (not synched_data->backtrace.push_if_enabled(record))
        {
            details::queue_push(synched_data->history, options.max_records_count, std::move(record));
        }
    }

    inline auto LogHandler_History::enable_backtrace(size_t record_buffer_size) -> void
    {
        assert(pimpl);
        pimpl->data->backtrace.set_max_trace(record_buffer_size);
    }

    inline auto LogHandler_History::disable_backtrace() -> void
    {
        assert(pimpl);
        pimpl->data->backtrace.disable();
    }

    inline auto LogHandler_History::log_backtrace() -> void
    {
        assert(pimpl);
        auto synched_data = pimpl->data.synchronize();
        for (auto& log : synched_data->backtrace)
        {
            details::queue_push(synched_data->history, options.max_records_count, std::move(log));
        }

        synched_data->backtrace.clear();
    }

    inline auto LogHandler_History::log_backtrace_no_guards() -> void
    {
        assert(pimpl);
        log_backtrace();  // Similar in this context
    }

    inline auto LogHandler_History::flush(std::optional<log_source>) -> void
    {
        assert(pimpl);
        // nothing to do, we keep history, there is no flush
    }

    inline auto LogHandler_History::set_flush_threshold(log_level) -> void
    {
        assert(pimpl);
        // nothing to do, we keep history, there is no flush
    }

    inline auto LogHandler_History::capture_history() const -> std::vector<LogRecord>
    {
        if (pimpl)
        {
            auto synched_data = pimpl->data.synchronize();
            return std::vector<LogRecord>(synched_data->history.begin(), synched_data->history.end());
        }

        return {};
    }

    inline auto LogHandler_History::clear_history()
    {
        if (pimpl)
        {
            pimpl->data->history.clear();
        }
    }

    auto LogHandler_History::is_started() const -> bool
    {
        return pimpl != nullptr;
    }

    //////////////////////////////////////////////////////////////////////////////////////

    inline LogHandler_StdOut::LogHandler_StdOut(std::ostream& out_, Options options_)
        : out(&out_)
        , options(std::move(options_))
    {
        assert(out);
    }

    inline LogHandler_StdOut::LogHandler_StdOut(LogHandler_StdOut&& other) noexcept
        : out(std::exchange(other.out, nullptr))
        , pimpl(std::move(other.pimpl))
        , options(std::move(other.options))
    {
    }

    inline LogHandler_StdOut& LogHandler_StdOut::operator=(LogHandler_StdOut&& other) noexcept
    {
        out = std::exchange(other.out, nullptr);
        pimpl = std::move(other.pimpl);
        options = std::move(other.options);
        return *this;
    }

    inline auto
    LogHandler_StdOut::start_log_handling(LoggingParams params, const std::vector<log_source>&)
        -> void
    {
        assert(out);

        if (not pimpl)
        {
            pimpl = std::make_unique<Impl>();
        }

        pimpl->current_log_level = params.logging_level;
        pimpl->backtrace->set_max_trace(params.log_backtrace);
    }

    inline auto LogHandler_StdOut::stop_log_handling(stop_reason) -> void
    {
        assert(out);
        assert(pimpl);

        if (options.clear_on_stop)
        {
            pimpl.reset();
        }
    }

    inline auto LogHandler_StdOut::set_log_level(log_level new_level) -> void
    {
        assert(out);
        assert(pimpl);

        pimpl->current_log_level = new_level;
    }

    inline auto LogHandler_StdOut::set_params(LoggingParams new_params) -> void
    {
        assert(out);
        assert(pimpl);

        pimpl->current_log_level = new_params.logging_level;
        pimpl->backtrace->set_max_trace(new_params.log_backtrace);
    }

    inline auto LogHandler_StdOut::log(LogRecord record) -> void
    {
        assert(out);
        assert(pimpl);

        if (pimpl->current_log_level > record.level)
        {
            return;
        }

        auto level = record.level;

        if (not pimpl->backtrace->push_if_enabled(record))
        {
            details::log_to_stream(*out, record, { .with_location = pimpl->log_location });
        }

        if (level <= pimpl->flush_threshold)
        {
            out->flush();
        }
    }

    inline auto LogHandler_StdOut::enable_backtrace(size_t record_buffer_size) -> void
    {
        assert(out);
        assert(pimpl);

        pimpl->backtrace->set_max_trace(record_buffer_size);
    }

    inline auto LogHandler_StdOut::disable_backtrace() -> void
    {
        assert(out);
        assert(pimpl);

        pimpl->backtrace->disable();
    }

    inline auto LogHandler_StdOut::log_backtrace() -> void
    {
        assert(out);
        assert(pimpl);

        auto synched_backtrace = pimpl->backtrace.synchronize();
        for (auto& log_record : *synched_backtrace)
        {
            details::log_to_stream(*out, log_record, { .with_location = pimpl->log_location });
        }
        synched_backtrace->clear();
    }

    inline auto LogHandler_StdOut::log_backtrace_no_guards() -> void
    {
        assert(out);
        assert(pimpl);

        log_backtrace();  // Similar in this context
    }

    inline auto LogHandler_StdOut::flush(std::optional<log_source>) -> void
    {
        assert(out);
        assert(pimpl);

        out->flush();
    }

    inline auto LogHandler_StdOut::set_flush_threshold(log_level threshold_level) -> void
    {
        assert(out);
        assert(pimpl);

        pimpl->flush_threshold = threshold_level;
    }

    auto LogHandler_StdOut::is_started() const -> bool
    {
        return pimpl != nullptr;
    }

}
