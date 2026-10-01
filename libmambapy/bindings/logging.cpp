// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <pybind11/native_enum.h>

#include "mamba/core/logging.hpp"
#include "mamba/core/logging_tools.hpp"

#include "bindings.hpp"

namespace mambapy
{
    void bind_submodule_logging(pybind11::module_ m)
    {
        namespace py = pybind11;

        namespace logging = mamba::logging;

        {
            static constexpr auto doc_log_level = R"(Level of logging, used to filter out logs which are at a lower level than the current one.
    - see `libmambapy.logging.LoggingParams`
    - see `libmambapy.logging.LogRecord`
    - see `libmambapy.logging.set_log_level`)";
            py::native_enum<mamba::log_level>(m, "LogLevel", "enum.Enum", doc_log_level)
                .value("TRACE", mamba::log_level::trace)
                .value("DEBUG", mamba::log_level::debug)
                .value("INFO", mamba::log_level::info)
                .value("WARNING", mamba::log_level::warn)
                .value("ERROR", mamba::log_level::err)
                .value("CRITICAL", mamba::log_level::critical)
                .value("OFF", mamba::log_level::off)
                .value("ALL", mamba::log_level::all)
                .finalize();


            m.def(
                "name_of_level",
                [](mamba::log_level value)
                {
                    // NOTE: this is necessary because this function is constexpr and doesnt have a
                    // runtime address until instanciated here.
                    return mamba::name_of(value);
                },
                "Provides the name that will be used in `LogRecord`s for the specified LogLevel.",
                py::arg("log_level")
            );
        }

        {
            static constexpr auto doc_log_source = R"(Specifies the source a `LogRecord` is originating from.
This is mainly useful for debugging issues coming from dependencies that have logging callbacks.)";
            py::native_enum<mamba::log_source>(m, "LogSource", "enum.Enum", doc_log_source)
                .value("LIBMAMBA", mamba::log_source::libmamba)
                .value("LIBCURL", mamba::log_source::libcurl)
                .value("LIBSOLV", mamba::log_source::libsolv)
                .value("TESTS", mamba::log_source::tests)
                .finalize();


            m.def(
                "name_of_source",
                [](mamba::log_source value)
                {
                    // NOTE: this is necessary because this function is constexpr and doesnt have a
                    // runtime address until instanciated here.
                    return mamba::name_of(value);
                },
                "Provides the name that will be used in `LogRecord`s for the specified LogLevel.",
                py::arg("log_level")
            );
        }

        {
            static constexpr auto
                doc_loggingparams_logging_level = "Minimum level a log record must have to not be filtered out.";
            static constexpr auto doc_loggingparams_log_backtrace = R"(Number of log records to keep in the backtrace history.
The backtrace feature will be enabled only if the value is different from `0`.)";
            static constexpr auto default_loggingparams = logging::LoggingParams{};
            py::class_<logging::LoggingParams>(m, "LoggingParams", "Parameters for the logging system.")
                .def(
                    py::init(
                        [](decltype(logging::LoggingParams::logging_level) logging_level,
                           decltype(logging::LoggingParams::log_backtrace) log_backtrace) -> logging::LoggingParams
                        {
                            return logging::LoggingParams{
                                .logging_level = std::move(logging_level),
                                .log_backtrace = std::move(log_backtrace),
                            };
                        }
                    ),
                    py::arg("logging_level") = default_loggingparams.logging_level,
                    py::arg("log_backtrace") = default_loggingparams.log_backtrace
                )
                .def_readwrite(
                    "logging_level",
                    &logging::LoggingParams::logging_level,
                    doc_loggingparams_logging_level
                )
                .def_readwrite(
                    "log_backtrace",
                    &logging::LoggingParams::log_backtrace,
                    doc_loggingparams_log_backtrace
                );
        }

        {
            static constexpr auto doc_logrecord = R"(All the information about a log.
    - see `libmamba.logging.log`
    - see `libmamba.logging.AnyLogHandler.log`)";

            py::class_<logging::LogRecord>(m, "LogRecord", doc_logrecord)
                .def(
                    py::init(
                        [](decltype(logging::LogRecord::message) message,
                           decltype(logging::LogRecord::level) level,
                           decltype(logging::LogRecord::source) source,
                           decltype(logging::LogRecord::location) location) -> logging::LogRecord
                        {
                            return logging::LogRecord{
                                .message = message,
                                .level = level,
                                .source = source,
                                .location = location,
                            };
                        }
                    )
                )
                .def_readwrite(
                    "message",
                    &logging::LogRecord::message,
                    "Message to be printed/captured in the logging implementation."
                )
                .def_readwrite(
                    "level",
                    &logging::LogRecord::level,
                    "Level of this log. If lower than the current level, this log will be ignored."
                )
                .def_readwrite("source", &logging::LogRecord::source, "Origin of this log.")
                .def_readwrite(
                    "location",
                    &logging::LogRecord::location,
                    "Source location of this log if available, otherwise empty."
                );
            // TODO: add equality comparison?
        }

        {

        }
    }
}