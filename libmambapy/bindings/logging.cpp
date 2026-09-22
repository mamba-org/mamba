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
                
        // static constexpr auto doc_log_level = R"(Level of logging, used to filter out logs which are at a lower level than the current one.
        // - see `libmambapy.logging.LoggingParams`
        // - see `libmambapy.logging.LogRecord`
        // - see `libmambapy.logging.set_log_level`)";
        // py::native_enum<mamba::log_level>(m, "LogLevel", "enum.Enum", doc_log_level)
        //     .value("TRACE", mamba::log_level::trace)
        //     .value("DEBUG", mamba::log_level::debug)
        //     .value("INFO", mamba::log_level::info)
        //     .value("WARNING", mamba::log_level::warn)
        //     .value("ERROR", mamba::log_level::err)
        //     .value("CRITICAL", mamba::log_level::critical)
        //     .value("OFF", mamba::log_level::off)
        //     .value("ALL", mamba::log_level::all)
        //     .finalize();
        

        m.def("name_of", [](mamba::log_level value){
                // NOTE: this is necessary because this function is constexpr and doesnt have a runtime address until instanciated here.
                return mamba::name_of(value);
            }, 
            "Provides the name that will be used in `LogRecord`s for the specified LogLevel.",
            py::arg("log_level"));
        
    }
}