// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.
#include <mutex>
#include <utility>
#include <vector>

#include <mamba/core/context.hpp>
#include <mamba/core/error_handling.hpp>
#include <mamba/core/execution.hpp>
#include <mamba/core/logging.hpp>
#include <mamba/core/output.hpp>  // TODO: remove
#include <mamba/core/util.hpp>
#include <mamba/util/synchronized_value.hpp>

namespace mamba::logging
{
    namespace
    {
        // We need the mutex to be constructible compile-time to allow using `constinit`
        // and avoid static-init-fiasco.
        // But depending on the implementation it is not always possible, so we pay
        // the locking cost in these platforms.
#if defined(__apple_build_version__) // apple-clang
        using params_mutex = std::mutex;
#else
        using params_mutex = std::shared_mutex;
#endif
        constinit util::synchronized_value<LoggingParams, params_mutex> logging_params;

        // IMPRTANT NOTE:
        // The handler MUST NOT be protected from concurrent calls at this level
        // as that would add high performance cost to logging from multiple threads.
        // Instead, we expect the implementation to handle concurrent calls correctly
        // in a thread-safe manner which is appropriate for this implementation.
        //
        // There are many ways to implement such handlers, including with a mutex,
        // various mutex types, through a concurrent lock-free queue of logging record feeding
        // a specific thread responsible for the output and file writing, etc.
        // There are too many ways that we can't predict and each impl will have it's own
        // best strategy.
        //
        // So instead of protecting at this level, we require the implementation of the handler
        // to be thread-safe, whatever the details. We can't enforce that property and can only
        // require it with the documentation which should guide the implementers anyway.
        constinit AnyLogHandler current_log_handler;

        // FIXME: maybe generalize and move in synchronized_value.hpp
        template <std::default_initializable T, typename U, typename... OtherArgs>
            requires std::assignable_from<T&, U>
        auto synchronize_with_value(
            util::synchronized_value<T, OtherArgs...>& sv,
            const std::optional<U>& new_value
        )
        {
            auto synched_value = sv.synchronize();
            if (new_value)
            {
                *synched_value = *new_value;
            }
            return synched_value;
        }
    }

    auto set_log_handler(AnyLogHandler new_handler, std::optional<LoggingParams> maybe_new_params)
        -> AnyLogHandler
    {
        if (current_log_handler)
        {
            current_log_handler.stop_log_handling();
        }

        auto previous_handler = std::exchange(current_log_handler, std::move(new_handler));

        auto params = synchronize_with_value(logging_params, maybe_new_params);

        if (current_log_handler)
        {
            current_log_handler.start_log_handling(*params, all_log_sources());
        }

        return previous_handler;
    }

    auto get_log_handler() -> AnyLogHandler&
    {
        return current_log_handler;
    }

    auto set_log_level(log_level new_level) -> log_level
    {
        auto synched_params = logging_params.synchronize();
        const auto previous_level = synched_params->logging_level;
        synched_params->logging_level = new_level;
        if (current_log_handler)
        {
            current_log_handler.set_log_level(synched_params->logging_level);
        }
        return previous_level;
    }

    auto get_log_level() noexcept -> log_level
    {
        return logging_params->logging_level;
    }

    auto get_logging_params() noexcept -> LoggingParams
    {
        return logging_params.value();
    }

    auto set_logging_params(LoggingParams new_params)
    {
        auto synched_params = logging_params.synchronize();
        *synched_params = std::move(new_params);
        if (current_log_handler)
        {
            current_log_handler.set_params(*synched_params);
        }
    }

    ///////////////////////////////////////////////////////////////////
    // MessageLogger
    namespace
    {
        constinit std::atomic<bool> message_logger_use_buffer;

        using MessageLoggerBuffer = std::vector<LogRecord>;
        constinit util::synchronized_value<MessageLoggerBuffer> message_logger_buffer;

        auto make_safe_log_record(std::string_view message, log_level level, std::source_location location)
        {
            // THINK: maybe remove as much locals as possible to enable optimizations with
            // temporaries
            // TODO: use fmt or std::format to do the space prepend
            const auto secured_message = Console::hide_secrets(message);
            auto formatted_message = prepend(secured_message, "", std::string(4, ' ').c_str());
            return LogRecord{
                .message = std::move(formatted_message),   //
                .level = level,                 //
                .source = log_source::libmamba, // default logging source, other sources will log
                                                // through other mechanisms
                .location = std::move(location) //
            };
        }

    }

    MessageLogger::MessageLogger(log_level level, std::source_location location)
        : m_level(level)
        , m_location(std::move(location))
    {
    }

    MessageLogger::~MessageLogger()
    {
        auto log_record = make_safe_log_record(m_stream.str(), m_level, std::move(m_location));
        if (!message_logger_use_buffer && Console::is_available())
        {
            emit(std::move(log_record));
        }
        else
        {
            message_logger_buffer->push_back(std::move(log_record));
        }
    }

    void MessageLogger::emit(LogRecord log_record)
    {
        logging::log(std::move(log_record));

        if (log_record.level == log_level::critical and get_log_level() != log_level::off)  // WARNING:
                                                                                           // THERE
                                                                                 // IS A LOCK HERE!
        {
            log_backtrace();
        }
    }

    void MessageLogger::activate_buffer()
    {
        message_logger_use_buffer = true;
    }

    void MessageLogger::deactivate_buffer()
    {
        message_logger_use_buffer = false;
    }

    void MessageLogger::print_buffer(std::ostream& /*ostream*/)
    {
        MessageLoggerBuffer tmp;
        message_logger_buffer->swap(tmp);

        for (auto& log_record : tmp)
        {
            emit(std::move(log_record));
        }

        flush_logs();
    }


}