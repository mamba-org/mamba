// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.
#include <atomic>
#include <mutex>
#include <ranges>
#include <vector>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <mamba/core/context.hpp>
#include <mamba/core/execution.hpp>
#include <mamba/core/logging_spdlog.hpp>
#include <mamba/core/tasksync.hpp>
#include <mamba/core/util.hpp>

namespace mamba::logging::spdlogimpl
{
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

    struct LogHandler_spdlog::Impl
    {
        TaskSynchronizer tasksync;
        std::atomic_bool is_active{ false };
    };

    LogHandler_spdlog::LogHandler_spdlog()
        : pimpl(std::make_unique<Impl>())
    {
    }

    LogHandler_spdlog::~LogHandler_spdlog() = default;

    LogHandler_spdlog::LogHandler_spdlog(LogHandler_spdlog&& other) noexcept = default;
    LogHandler_spdlog& LogHandler_spdlog::operator=(LogHandler_spdlog&& other) noexcept = default;

    auto
    LogHandler_spdlog::start_log_handling(const LoggingParams params, std::vector<log_source> sources)
        -> void
    {
        assert(pimpl);
        if (sources.empty())
        {
            throw std::invalid_argument("LogHandler_spdlog must be started with at least one log source"
            );
        }

        const auto main_source = sources.front();

        spdlog::set_default_logger(
            std::make_shared<Logger>(name_of(main_source), params.log_pattern, "\n")
        );
        MainExecutor::instance().on_close(pimpl->tasksync.synchronized(
            []
            {
                if (auto logger = spdlog::default_logger())
                {
                    logger->flush();
                }
            }
        ));

        for (const auto source : sources | std::views::drop(1))
        {
            spdlog::register_logger(std::make_shared<Logger>(name_of(source), params.log_pattern, ""));
        }

        spdlog::set_level(to_spdlog(params.logging_level));

        pimpl->is_active = true;
    }

    auto LogHandler_spdlog::stop_log_handling(stop_reason reason) -> void
    {
        if (not pimpl)
        {
            return;
        }

        pimpl->tasksync.join_tasks();

        // BEWARE:
        // When exiting the program, we need to let spdlog handle that
        // gracefully by itself.
        // spdlog should flush and properly cleanup, but here we cannot
        // guarantee if spdlog has been shutdown or not already, which
        // can lead to crashes if we try to do anything with spdlog
        // after it has been shutdown.
        // Instead we do nothing when we are exiting the program,
        // otherwise we need to flush and unregister loggers.
        if (reason != stop_reason::program_exit)
        {
            if (auto default_logger = spdlog::default_logger())
            {
                default_logger->flush();
            }

            spdlog::drop_all();
            pimpl->tasksync.reset();
        }
        pimpl->is_active = false;
    }

    namespace
    {
        template <std::invocable<std::shared_ptr<spdlog::logger>> Func>
        auto apply_to_logger(log_source source, Func&& func) -> void
        {
            if (auto logger = spdlog::get(name_of(source)))
            {
                std::invoke(std::forward<Func>(func), std::move(logger));
            }
            else
            {
                auto default_logger = spdlog::default_logger();
                assert(default_logger);
                default_logger->log(
                    to_spdlog(log_level::err),
                    "spdlog logger for source {} not found - operation skipped",
                    name_of(source)
                );
            }
        }
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
        apply_to_logger(
            record.source,
            [&](auto logger)
            {
                logger->log(
                    spdlog::source_loc{
                        record.location.file_name(),
                        static_cast<int>(record.location.line()),  // CRINGE
                        record.location.function_name(),
                    },
                    to_spdlog(record.level),
                    record.message
                );
            }
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

    auto LogHandler_spdlog::log_backtrace() -> void
    {
        spdlog::dump_backtrace();
    }

    auto LogHandler_spdlog::log_backtrace_no_guards() -> void
    {
        auto logger = spdlog::default_logger();
        assert(logger);

        auto plogger = static_cast<Logger*>(logger.get());
        plogger->dump_backtrace_no_guards();
    }

    auto LogHandler_spdlog::set_flush_threshold(log_level threshold_level) -> void
    {
        spdlog::flush_on(to_spdlog(threshold_level));
    }

    auto LogHandler_spdlog::flush(std::optional<log_source> source) -> void
    {
        if (source)
        {
            apply_to_logger(*source, [](auto logger) { logger->flush(); });
        }
        else
        {
            spdlog::apply_all([](std::shared_ptr<spdlog::logger> l) { l->flush(); });
        }
    }

    auto LogHandler_spdlog::is_started() const -> bool
    {
        return pimpl and pimpl->is_active;
    }

}