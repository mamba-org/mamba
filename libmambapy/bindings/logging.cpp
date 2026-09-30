// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "bindings.hpp"

#include <pybind11/native_enum.h>

#include "mamba/core/logging.hpp"
#include "mamba/core/logging_tools.hpp"

namespace mambapy
{
    void bind_submodule_logging(pybind11::module_ m)
    {
        namespace py = pybind11;
        namespace logging = mamba::logging;
                
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
        

        m.def("name_of", [](mamba::log_level value){
                // NOTE: this is necessary because this function is constexpr and doesnt have a runtime address until instanciated here.
                return mamba::name_of(value);
            }, 
            "Provides the name that will be used in `LogRecord`s for the specified LogLevel.",
            py::arg("log_level"));
        
        static constexpr auto doc_loggingparams_logging_level = "Minimum level a log record must have to not be filtered out.";
        static constexpr auto doc_loggingparams_log_backtrace = R"(Number of log records to keep in the backtrace history.
The backtrace feature will be enabled only if the value is different from `0`.)";
        static constexpr auto default_loggingparams = logging::LoggingParams{};
        py::class_<logging::LoggingParams>(m, "LoggingParams", "Parameters for the logging system.")
            .def(
                py::init(
                    [](decltype(logging::LoggingParams::logging_level) logging_level,
                    decltype(logging::LoggingParams::log_backtrace) log_backtrace) -> logging::LoggingParams
                    {
                        // TODO: improve this, see https://wg21.link/p2287 for the reason
                        logging::LoggingParams params;
                        params.logging_level = std::move(logging_level);
                        params.log_backtrace = std::move(log_backtrace);
                        return params;
                    }
                ),
                py::arg("logging_level") = default_loggingparams.logging_level,
                py::arg("log_backtrace") = default_loggingparams.log_backtrace
            )
            .def_readwrite("logging_level", &logging::LoggingParams::logging_level, doc_loggingparams_logging_level)
            .def_readwrite("log_backtrace", &logging::LoggingParams::log_backtrace, doc_loggingparams_log_backtrace);

        
    }
}