// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_LOGGING_SPDLOG_HPP
#define MAMBA_CORE_LOGGING_SPDLOG_HPP

#include <memory>
#include <vector>

#include <spdlog/common.h>

#include <mamba/core/logging.hpp>

namespace mamba
{
    class TaskSynchronizer;
}

namespace mamba::logging::spdlogimpl
{


    // THINK: add namespace?
    inline constexpr auto to_spdlog(log_level level) -> spdlog::level::level_enum
    {
        static_assert(sizeof(log_level) == sizeof(spdlog::level::level_enum));
        static_assert(static_cast<int>(log_level::all) == static_cast<int>(spdlog::level::level_enum::n_levels));
        return static_cast<spdlog::level::level_enum>(level);
    }

    class LogHandler_spdlog
    {
    public:

        LogHandler_spdlog();
        ~LogHandler_spdlog();

        LogHandler_spdlog(LogHandler_spdlog&& other);
        LogHandler_spdlog& operator=(LogHandler_spdlog&& other);

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

    private:

        std::unique_ptr<TaskSynchronizer> tasksync;
    };

    static_assert(logging::LogHandler<LogHandler_spdlog>);

}

#endif
