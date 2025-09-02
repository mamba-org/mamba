// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

#include <fmt/core.h>  // TODO: replace by `<format>` once available on all ci compilers

#include <mamba/core/context.hpp>
#include <mamba/core/error_handling.hpp>
#include <mamba/core/execution.hpp>
#include <mamba/core/invoke.hpp>
#include <mamba/core/logging.hpp>
#include <mamba/core/output.hpp>  // TODO: remove
#include <mamba/core/util.hpp>
#include <mamba/util/synchronized_value.hpp>

namespace mamba::logging
{

    namespace details
    {
        // see singletons.cpp
#if defined(__APPLE__)
        using params_mutex = std::mutex;
#else
        using params_mutex = std::shared_mutex;
#endif
        extern util::synchronized_value<LoggingParams, params_mutex> logging_params;
        extern AnyLogHandler current_log_handler;
    }

    namespace
    {
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

    AnyLogHandler::~AnyLogHandler()
    {
        // We handle the case where we don't exit normally (calling `exit(0);` for example)
        // but we still need to properly end the logging system.
        // If `this` is the currently registered log-handler, stop it properly before
        // calling the implementation's destruction.
        if (has_value() and this == &details::current_log_handler)
        {
            static std::once_flag flag;

            // clang-format off
            std::call_once(flag, [&]{
                safe_invoke([this] { this->stop_log_handling(); })
                    .map_error([](const mamba_error& error){
                            // Here with report the error in the standard output to avoid any logging
                            // implementation.
                            const auto message = fmt::format(
                                "mamba::logging termination failure: call to `stop_log_handling()` ended with an error (caught, logged, skiped): {}",
                                error.what()
                            );
                            std::cerr << message << std::endl;
                            std::cout << message << std::endl;
                        }
                    );
            });
            // clang-format on
        }
    }

    auto set_log_handler(AnyLogHandler new_handler, std::optional<LoggingParams> maybe_new_params)
        -> AnyLogHandler
    {
        if (details::current_log_handler)
        {
            details::current_log_handler.stop_log_handling();
        }

        auto previous_handler = std::exchange(details::current_log_handler, std::move(new_handler));

        auto params = synchronize_with_value(details::logging_params, maybe_new_params);

        if (details::current_log_handler)
        {
            details::current_log_handler.start_log_handling(*params, all_log_sources());
        }

        return previous_handler;
    }

    auto get_log_handler() -> AnyLogHandler&
    {
        return details::current_log_handler;
    }

    auto set_log_level(log_level new_level) -> log_level
    {
        auto synched_params = details::logging_params.synchronize();
        const auto previous_level = synched_params->logging_level;
        synched_params->logging_level = new_level;
        if (details::current_log_handler)
        {
            details::current_log_handler.set_log_level(synched_params->logging_level);
        }
        return previous_level;
    }

    auto get_log_level() /*noexcept*/ -> log_level
    {
        return details::logging_params->logging_level;
    }

    auto get_logging_params() /*noexcept*/ -> LoggingParams
    {
        return details::logging_params.value();
    }

    auto set_logging_params(LoggingParams new_params) -> LoggingParams
    {
        auto synched_params = details::logging_params.synchronize();
        LoggingParams previous_params = *synched_params;
        *synched_params = std::move(new_params);
        if (details::current_log_handler)
        {
            details::current_log_handler.set_params(*synched_params);
        }

        return previous_params;
    }

    ///////////////////////////////////////////////////////////////////
    // MessageLogger
    namespace details
    {
        // see singletons.cpp
        extern std::atomic<bool> message_logger_use_buffer;
        extern util::synchronized_value<MessageLoggerBuffer> message_logger_buffer;
    }

    namespace
    {
        auto
        make_safe_log_record(std::string_view message, log_level level, std::source_location location)
        {
            // THINK: maybe remove as much locals as possible to enable optimizations with
            // temporaries
            // TODO: use fmt or fmt::format to do the space prepend
            const auto secured_message = Console::hide_secrets(message);
            auto formatted_message = prepend(secured_message, "", std::string(4, ' ').c_str());
            return LogRecord{
                .message = std::move(formatted_message),  //
                .level = level,                           //
                .source = log_source::libmamba,  // default logging source, other sources will log
                                                 // through other mechanisms
                .location = std::move(location)  //
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
        if (!details::message_logger_use_buffer && Console::is_available())
        {
            emit(std::move(log_record));
        }
        else
        {
            details::message_logger_buffer->ready_records().push_back(std::move(log_record));
        }
    }

    void MessageLogger::emit(LogRecord log_record)
    {
        logging::log(std::move(log_record));

        // BEWARE: `get_log_level()` implies a (maybe shared) mutex lock, so it is important
        //         to avoid calling it until we must, that is, always AFTER verifying that
        //         we have a critical log.
        if (log_record.level == log_level::critical and get_log_level() != log_level::off)
        {
            log_backtrace();
        }
    }

    void MessageLogger::activate_buffer()
    {
        details::message_logger_use_buffer = true;
    }

    void MessageLogger::deactivate_buffer()
    {
        details::message_logger_use_buffer = false;
    }

    void MessageLogger::print_buffer(std::ostream& /*out*/)
    {
        details::MessageLoggerBuffer::buffer tmp;
        details::message_logger_buffer->ready_records().swap(tmp);

        for (auto& log_record : tmp)
        {
            emit(std::move(log_record));
        }

        flush_logs();
    }


}
