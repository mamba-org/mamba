// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.
#include <mamba/core/logging_spdlog.hpp>

#include <vector>
#include <mutex>

#include <mamba/core/context.hpp>
#include <mamba/core/output.hpp> // TODO: remove
#include <mamba/core/util.hpp>
#include <mamba/core/execution.hpp>
#include <mamba/core/tasksync.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>


namespace mamba
{
    // FIXME: merge scoped logger with this type, they are the samem, scopepd logger as introduced to patch logger
    class Logger : public spdlog::logger
    {
    public:

        Logger(const std::string& name, const std::string& pattern, const std::string& eol);

        void dump_backtrace_no_guards();
    };

    Logger::Logger(
        const std::string& name,
        const std::string& pattern,
        const std::string& eol
    )
        : spdlog::logger(name, std::make_shared<spdlog::sinks::stderr_color_sink_mt>())
    {
        auto f = std::make_unique<spdlog::pattern_formatter>(
            pattern,
            spdlog::pattern_time_type::local,
            eol
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
    class ScopedLogger
    {
        std::shared_ptr<Logger> m_logger;

    public:

        explicit ScopedLogger(
            std::shared_ptr<Logger> new_logger,
            logger_kind kind = logger_kind::normal_logger
        )
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



    struct LogHandler_spdlog::Impl
    {
        std::vector<ScopedLogger> loggers;
        TaskSynchronizer tasksync;
    };


    LogHandler_spdlog::LogHandler_spdlog() = default;
    LogHandler_spdlog::~LogHandler_spdlog() = default;


    auto LogHandler_spdlog::start_log_handling(const LoggingParams params, std::vector<log_source> sources) -> void
    {
        pimpl = std::make_unique<Impl>();

        const auto main_source = sources.front();
        sources.pop_back();

        pimpl->loggers.emplace_back(
            std::make_shared<Logger>(name_of(main_source), params.log_pattern, "\n"),
            logger_kind::default_logger
        );
        MainExecutor::instance().on_close(
            pimpl->tasksync.synchronized([this] { pimpl->loggers.front().logger()->flush(); })
        );

        for (const auto source : sources)
        {
            pimpl->loggers.emplace_back(
                std::make_shared<Logger>(name_of(source), params.log_pattern, "")
            );
        }

        spdlog::set_level(convert_log_level(params.logging_level));
    }



}