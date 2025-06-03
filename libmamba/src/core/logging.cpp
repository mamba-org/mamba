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
    namespace  // TODO: STATIC INIT FIASCO!!!
    {
        util::synchronized_value<LoggingParams> logging_params;

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
        // to be thread-safe, whatever the means. We can't enforce that property and can only
        // require it with the documentation which should guide the implementers anyway.
        AnyLogHandler current_log_handler;


        // FIXME: maybe generalize and move in synchronized_value.hpp
        template<std::default_initializable T, typename U, typename... OtherArgs>
            requires std::assignable_from<T&, U>
        auto
        synchronize_with_value(util::synchronized_value<T, OtherArgs...>& sv, const std::optional<U>& new_value)
        {
            auto synched_value = sv.synchronize();
            if (new_value)
            {
                *synched_value = *new_value;
            }
            return synched_value;
        }
    }


    auto set_log_handler(AnyLogHandler new_handler, std::optional<LoggingParams> maybe_params)
        -> AnyLogHandler
    {
        if (current_log_handler)
        {
            current_log_handler.stop_log_handling();
        }


        auto previous_handler = std::exchange(current_log_handler, std::move(new_handler));

        auto params = synchronize_with_value(logging_params, maybe_params);

        if (current_log_handler)
        {
            current_log_handler.start_log_handling(*params, all_log_sources());
        }
    }

    auto get_log_handler() -> const AnyLogHandler&
    {
        return current_log_handler;
    }

    auto set_log_level(log_level new_level) -> log_level
    {
        auto synched_params = logging_params.synchronize();
        const auto previous_level = synched_params->logging_level;
        synched_params->logging_level = new_level;
        current_log_handler.set_log_level(synched_params->logging_level);
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
        logging_params = std::move(new_params);
    }

    auto log(LogRecord record) noexcept -> void
    {
        current_log_handler.log(std::move(record));
    }

    auto log_stacktrace(std::optional<log_source> source) noexcept -> void
    {
        current_log_handler.log_stacktrace(std::move(source));
    }

    auto flush_logs(std::optional<log_source> source) noexcept -> void
    {
        current_log_handler.flush(std::move(source));
    }

    auto log_stacktrace_no_guards(std::optional<log_source> source) noexcept -> void
    {
        current_log_handler.log_stacktrace_no_guards(std::move(source));
    }

    ///////////////////////////////////////////////////////////////////
    // AnyLogHandler

    AnyLogHandler::AnyLogHandler() = default;
    AnyLogHandler::~AnyLogHandler() = default;


    ///////////////////////////////////////////////////////////////////
    // MessageLogger

    struct MessageLoggerData
    {
        static std::mutex m_mutex;
        static bool use_buffer;
        static std::vector<std::pair<std::string, log_level>> m_buffer;
    };

    MessageLogger::MessageLogger(log_level level)
        : m_level(level)
    {
    }

    MessageLogger::~MessageLogger()
    {
        if (!MessageLoggerData::use_buffer && Console::is_available())
        {
            emit(m_stream.str(), m_level);
        }
        else
        {
            const std::lock_guard<std::mutex> lock(MessageLoggerData::m_mutex);
            MessageLoggerData::m_buffer.push_back({ m_stream.str(), m_level });
        }
    }

    void MessageLogger::emit(const std::string& msg, const log_level& level)
    {
        // THINK: maybe remove as much locals as possible to enable optimizations with temporaries
        // TODO: use fmt or std::format to do the space prepend
        const auto secured_message = Console::hide_secrets(msg);
        const auto formatted_message = prepend(secured_message, "", std::string(4, ' ').c_str());
        logging::log(LogRecord{
            .message = formatted_message,   //
            .level = level,                 //
            .source = log_source::libmamba  //
        });

        if (level == log_level::critical and get_log_level() != log_level::off)
        {
            log_stacktrace();
        }
    }

    void MessageLogger::activate_buffer()
    {
        MessageLoggerData::use_buffer = true;
    }

    void MessageLogger::deactivate_buffer()
    {
        MessageLoggerData::use_buffer = false;
    }

    void MessageLogger::print_buffer(std::ostream& /*ostream*/)
    {
        decltype(MessageLoggerData::m_buffer) tmp;

        {
            const std::lock_guard<std::mutex> lock(MessageLoggerData::m_mutex);
            MessageLoggerData::m_buffer.swap(tmp);
        }

        for (const auto& [msg, level] : tmp)
        {
            emit(msg, level);
        }

        // TODO impl commented
        /*spdlog::apply_all([&](std::shared_ptr<spdlog::logger> l) { l->flush(); });*/
        flush_logs();
    }


}