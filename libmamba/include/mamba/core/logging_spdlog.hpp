// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_LOGGING_SPDLOG_HPP
#define MAMBA_CORE_LOGGING_SPDLOG_HPP

#include <memory>

#include <mamba/core/tasksync.hpp>
#include <mamba/core/logging.hpp>

#include <spdlog/common.h>

namespace mamba
{
    // THINK: add namespace?
    inline auto convert_log_level(log_level l) -> spdlog::level::level_enum
    {
        return static_cast<spdlog::level::level_enum>(l);
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

        auto log_stacktrace(std::optional<log_source> source = {}) -> void;
        auto log_stacktrace_no_guards(std::optional<log_source> source = {}) -> void;
        auto flush(std::optional<log_source> source = {}) -> void;

    private:

        struct Impl;
        std::unique_ptr<Impl> pimpl;

    };

    static_assert(logging::LogHandler<LogHandler_spdlog>);

}

#endif
