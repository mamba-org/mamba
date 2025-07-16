// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.
#include <mutex>
#include <vector>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <mamba/core/context.hpp>
#include <mamba/core/execution.hpp>
#include <mamba/core/logging_spdlog.hpp>
#include <mamba/core/output.hpp>  // TODO: remove
#include <mamba/core/tasksync.hpp>
#include <mamba/core/util.hpp>

namespace mamba
{
    // FIXME: merge scoped logger with this type, they are the samem, scopepd logger as introduced
    // to patch logger
    class Logger : public spdlog::logger
    {
    public:

        Logger(std::string_view name, std::string_view pattern, std::string_view eol);

        void dump_backtrace_no_guards();
    };

    Logger::Logger(std::string_view name, std::string_view pattern, std::string_view eol)
        : spdlog::logger(std::string(name), std::make_shared<spdlog::sinks::stderr_color_sink_mt>())
    {
        auto f = std::make_unique<spdlog::pattern_formatter>(
            std::string(pattern),
            spdlog::pattern_time_type::local,
            std::string(eol)
        );
        set_formatter(std::move(f));
    }

    void Logger::dump_backtrace_no_guards()
    {
        using spdlog::details::log_msg;
        if (tracer_.enabled())
        {
            tracer_.foreach_pop(
                [this](const log_msg& msg)
                {
                    if (this->should_log(msg.level))
                    {
                        this->sink_it_(msg);
                    }
                }
            );
        }
    }


    enum class logger_kind
    {
        normal_logger,
        default_logger,
    };

    // Associate the registration of a logger to the lifetime of this object.
    // This is used to help with making sure loggers are unregistered once
    // their logical owner is destroyed.
    class LogHandler_spdlog::ScopedLogger
    {
        std::shared_ptr<Logger> m_logger;

    public:

        explicit ScopedLogger(std::shared_ptr<Logger> new_logger, logger_kind kind = logger_kind::normal_logger)
            : m_logger(std::move(new_logger))
        {
            assert(m_logger);
            if (kind == logger_kind::default_logger)
            {
                spdlog::set_default_logger(m_logger);
            }
            else
            {
                spdlog::register_logger(m_logger);
            }
        }

        ~ScopedLogger()
        {
            if (m_logger)
            {
                spdlog::drop(m_logger->name());
            }
        }

        std::shared_ptr<Logger> logger() const
        {
            assert(m_logger);
            return m_logger;
        }

        ScopedLogger(ScopedLogger&&) = default;
        ScopedLogger& operator=(ScopedLogger&&) = default;

        ScopedLogger(const ScopedLogger&) = delete;
        ScopedLogger& operator=(const ScopedLogger&) = delete;
    };

    LogHandler_spdlog::LogHandler_spdlog()
        : tasksync(std::make_unique<TaskSynchronizer>())
    {

    }

    LogHandler_spdlog::~LogHandler_spdlog() = default;

    LogHandler_spdlog::LogHandler_spdlog(LogHandler_spdlog&& other) = default;
    LogHandler_spdlog& LogHandler_spdlog::operator=(LogHandler_spdlog&& other) = default;

    auto LogHandler_spdlog::get_logger(log_source source) -> ScopedLogger&
    {
        // THINK: consider only using spdlog to get the loggers
        const auto logger_idx = static_cast<size_t>(source);
        assert(logger_idx > 0 && logger_idx < loggers.size());
        auto& logger = loggers[logger_idx];
        assert(logger.logger());
        return logger;
    }

    auto LogHandler_spdlog::default_logger() -> ScopedLogger&
    {
        return get_logger(log_source::libmamba);
    }

    auto
    LogHandler_spdlog::start_log_handling(const LoggingParams params, std::vector<log_source> sources)
        -> void
    {
        assert(tasksync);
        assert(sources.size() > 0);

        const auto main_source = sources.front();

        loggers.emplace_back(
            std::make_shared<Logger>(name_of(main_source), params.log_pattern, "\n"),
            logger_kind::default_logger
        );
        MainExecutor::instance().on_close(
            tasksync->synchronized([this] { loggers.front().logger()->flush(); })
        );

        for (const auto source : sources | std::views::drop(1) )
        {
            loggers.emplace_back(std::make_shared<Logger>(name_of(source), params.log_pattern, ""));
        }

        spdlog::set_level(to_spdlog(params.logging_level));
    }

    auto LogHandler_spdlog::stop_log_handling() -> void
    {
        loggers.clear();
        spdlog::shutdown();  // ? or drop_all?
    }

    auto LogHandler_spdlog::set_log_level(log_level new_level) -> void
    {
        spdlog::set_level(to_spdlog(new_level));
    }

    auto LogHandler_spdlog::set_params(LoggingParams new_params) -> void
    {
        // TODO: add missing parameters
        spdlog::set_level(to_spdlog(new_params.logging_level));
    }

    auto LogHandler_spdlog::log(const logging::LogRecord record) -> void
    {
        // THINK: consider only using spdlog to get the loggers
        auto logger = get_logger(record.source).logger();
        logger->log(
            spdlog::source_loc{
                record.location.file_name(),
                static_cast<int>(record.location.line()), // CRINGE
                record.location.function_name(),
            },
            to_spdlog(record.level),
            record.message
        );
    }

    auto LogHandler_spdlog::enable_backtrace(size_t record_buffer_size) -> void
    {
        spdlog::enable_backtrace(record_buffer_size);
    }

    auto LogHandler_spdlog::disable_backtrace() -> void
    {
        spdlog::disable_backtrace();
    }

    auto LogHandler_spdlog::log_backtrace() noexcept -> void
    {
        spdlog::dump_backtrace();
    }

    auto LogHandler_spdlog::log_backtrace_no_guards() noexcept -> void
    {
        default_logger().logger()->dump_backtrace_no_guards();
    }

    auto LogHandler_spdlog::set_flush_threshold(log_level threshold_level) noexcept -> void
    {
        spdlog::flush_on(to_spdlog(threshold_level));
    }

    auto LogHandler_spdlog::flush(std::optional<log_source> source) -> void
    {
        if (source)
        {
            get_logger(source.value()).logger()->flush(); // THINK: consider only using spdlog to get the loggers
        }
        else
        {
            spdlog::apply_all([](std::shared_ptr<spdlog::logger> l) { l->flush(); });
        }
    }

}