// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <vector>
#include <unordered_map>

#include <pybind11/native_enum.h>
#include <pybind11/stl.h>

#include <fmt/std.h>

#include "mamba/core/logging.hpp"
#include "mamba/core/logging_tools.hpp"

#include "logging.hpp"



namespace mambapy
{
    namespace logging = mamba::logging;

    using InvalidInterfaceReport = std::vector<std::string>;

    // TODO: DOC!!!
    auto check_log_handler_interface(const pybind11::object& object, bool early_exit) -> std::optional<InvalidInterfaceReport>
    {
        using namespace std::literals;

        std::optional<InvalidInterfaceReport> result;

        const auto add_failure = [&](std::string failure_msg){
            if(not result) result.emplace();
            result->push_back(failure_msg);
        };

        struct FuncDesc
        {
            const char* name;
            const char* signature;
        };

        static constexpr std::array expected_member_descs{
            FuncDesc{ "start_log_handling", "(params: LogParams, sources: [LogSource])"},
            FuncDesc{ "stop_log_handling", "()"},
            FuncDesc{ "set_log_level", "(log_level: LogLevel)"},
            FuncDesc{ "set_params", "(params: LogParams)"},
            FuncDesc{ "log", "(record: LogRecord)"},
            FuncDesc{ "enable_bacltrace", "(size: int)"},
            FuncDesc{ "log_backtrace", "()"},
            FuncDesc{ "log_backtrace_no_guards", "()"},
            FuncDesc{ "flush", "(source: LogSource|None)"},
            FuncDesc{ "set_flush_threshold", "(level: LogLevel)"}
        };

        for (const auto& desc : expected_member_descs)
        {
            if(object.attr(desc.name).is_none())
            {
                add_failure(std::format("member not found: {}{}", desc.name, desc.signature));
                if(early_exit)
                {
                    break;
                }
            }
        }

        return result;
    }


    // TODO: DOC!!!
    auto has_log_handler_interface(const pybind11::object& object) -> bool
    {
        return not check_log_handler_interface(object, true).has_value();
    }

   auto validate_log_handler_interface(const pybind11::object& object) -> void
   {
        const auto maybe_errors = check_log_handler_interface(object, false);
        if(maybe_errors)
        {
            throw std::invalid_argument(fmt::format("object does not have a LogHandler interface : {}", maybe_errors.value()));
        }
   }

   namespace
   {
        bool is_log_handler_interface_validation_enabled = true;
   }

   auto enable_log_handler_interface_validation(bool enabled) -> void
   {
        is_log_handler_interface_validation_enabled = enabled;
   }

    namespace details
    {
        namespace {
            struct pytype_hash
            {
                auto operator()(const pybind11::type& type) const noexcept
                {
                    return pybind11::hash(type);
                }
            };

            struct pytype_equal
            {
                auto operator()(const pybind11::type& left, const pybind11::type& right) const -> bool
                {
                    return left.is(right);
                }
            };

            std::unordered_map<pybind11::type, AnyLogHandlerConstructor, pytype_hash, pytype_equal> registry_anyloghandler_constructors;

        }

        auto register_anyloghandler_constructor(
            pybind11::type type_id,
            AnyLogHandlerConstructor constructor
        ) -> void
        {
            registry_anyloghandler_constructors.emplace(type_id, constructor);
        }

        // note: we take `pyobject` by reference to be able to conditionally move it
        auto make_anyloghandler_from_cpp_loghandler_type(pybind11::object& pyobject) -> std::optional<mamba::logging::AnyLogHandler>
        {
            auto found_it = registry_anyloghandler_constructors.find(pybind11::type::of(pyobject));
            if (found_it == registry_anyloghandler_constructors.end())
            {
                return std::nullopt;
            }

            const auto& constructor = found_it->second;
            return constructor(std::move(pyobject));
        }
    }

    namespace
    {

        // TODO: DOC!!!
        // Wraps a Python object that provides `LogHandler` interface into a C++ type which can be
        // passed to `AnyLogHandler`.
        struct PyAnyLogHandler
        {
            pybind11::object impl;

            PyAnyLogHandler() = default;
            PyAnyLogHandler(const PyAnyLogHandler&) = delete;
            PyAnyLogHandler& operator=(const PyAnyLogHandler&) = delete;
            PyAnyLogHandler(PyAnyLogHandler&&) = default;
            PyAnyLogHandler& operator=(PyAnyLogHandler&&) = default;

            explicit PyAnyLogHandler(pybind11::object object)
                : impl(std::move(object))
            {
                if(is_log_handler_interface_validation_enabled)
                {
                    validate_log_handler_interface(impl);
                }
            }

            auto is_none() const -> bool
            {
                return impl.is_none();
            }

            auto is_valid() const -> bool
            {
                return not is_none();
            }

            auto start_log_handling(
                logging::LoggingParams params,
                const std::vector<logging::log_source>& sources
            ) -> void
            {
                impl.attr("start_log_handling")(params, sources);
            }

            auto stop_log_handling() -> void
            {
                impl.attr("stop_log_handling");
            }

            auto set_log_level(logging::log_level level) -> void
            {
                impl.attr("set_log_level")(level);
            }

            auto set_params(logging::LoggingParams new_params) -> void
            {
                impl.attr("set_params")(new_params);
            }

            auto log(logging::LogRecord record) -> void
            {
                impl.attr("log")(record);
            }

            auto enable_backtrace(std::size_t backtrace_size) -> void
            {
                impl.attr("enable_backtrace")(backtrace_size);
            }

            auto log_backtrace() -> void
            {
                impl.attr("log_backtrace");
            }

            auto log_backtrace_no_guards() -> void
            {
                impl.attr("log_backtrace_no_guards");
            }

            auto flush(std::optional<logging::log_source> source_to_flush = std::nullopt) -> void
            {
                impl.attr("flush")(source_to_flush);
            }

            auto set_flush_threshold(logging::log_level level_threshold = logging::log_level::all)
                -> void
            {
                impl.attr("set_flush_threshold")(level_threshold);
            }
        };

        static_assert(logging::LogHandler_Moveable<PyAnyLogHandler>);

        using loghandler_ptr = std::unique_ptr<logging::AnyLogHandler>;

        auto has_valid_object(const logging::AnyLogHandler& handler) -> bool
        {
            const auto stored_type_id = handler.type_id();
            if (stored_type_id == typeid(PyAnyLogHandler))
            {
                return handler.unsafe_get<PyAnyLogHandler>()->is_valid();
            }
            return stored_type_id.has_value();
        }

        auto has_python_object(const logging::AnyLogHandler& handler) -> bool
        {
            auto maybe_type_id = handler.type_id();
            std::cout << fmt::format("HACK: maybe_type_id = {}", maybe_type_id ? maybe_type_id->name() : "None") << std::endl; 
            return maybe_type_id and maybe_type_id.value() == typeid(PyAnyLogHandler);
        }

        auto get_python_object(const logging::AnyLogHandler& handler)
            -> std::optional<pybind11::object>
        {
            auto* py_object = handler.unsafe_get<PyAnyLogHandler>();
            if (not py_object or py_object->is_none())
            {
                return {};
            }

            return py_object->impl;
        }

        auto to_anyloghandler(pybind11::object pyobject) -> logging::AnyLogHandler
        {
            // When the object passed is actually a C++ `logging::AnyLogHandler`, we take it by
            // move. When it's any other object, we assume it's a python log handler and wrap it
            // appropriately.

            
            namespace py = pybind11;
            if(pyobject.is_none())
            {
                return logging::AnyLogHandler{};
            }

            const bool is_anyloghandler = py::isinstance<logging::AnyLogHandler>(pyobject);

            if (is_anyloghandler)
            {
                return std::move(pyobject).cast<logging::AnyLogHandler>();
            }
            if (auto maybe_cpp_handler = details::make_anyloghandler_from_cpp_loghandler_type(pyobject))
            {
                return std::move(maybe_cpp_handler).value();
            }
            else
            {
                return logging::AnyLogHandler{ PyAnyLogHandler{ pyobject } };
            }
        }

        void bind_any_log_handler(pybind11::module_ module)
        {
            namespace py = pybind11;

            constexpr auto doc_class = "TODO: DOCUMENTATION HERE.";
            py::class_<logging::AnyLogHandler>(module, "AnyLogHandler", doc_class)
                .def(
                    py::init(&to_anyloghandler),
                    py::arg("log_handler_impl") = py::none{},
                    py::return_value_policy::move
                )
                .def("has_value", &has_valid_object)
                .def("__bool__", &has_valid_object)
                .def("has_pyobject", &has_python_object)
                .def("get_pyobject", &get_python_object);  // THINK: add other member functions?

            module.def("check_log_handler_interface", &check_log_handler_interface);
            module.def("has_log_handler_interface", &has_log_handler_interface);
            module.def("validate_log_handler_interface", &validate_log_handler_interface);
            module.def("enable_log_handler_interface_validation", &enable_log_handler_interface_validation);
        }

        void bind_loghandler_history(pybind11::module_ module)
        {
            namespace py = pybind11;

            using Options = logging::LogHandler_History::Options;
            {
                constexpr auto doc_class = "TODO: DOCUMENTATION HERE.";

                py::class_<Options>(module, "LogHandler_History.Options", doc_class)
                    .def(
                        py::init(
                            [](size_t max_records_count, bool clear_on_stop)
                            {
                                return Options{ .max_records_count = max_records_count,
                                                .clear_on_stop = clear_on_stop };
                            }
                        ),
                        py::arg("max_records_count") = size_t{ 0 },
                        py::arg("clear_on_stop") = false
                    )
                    .def_readwrite("max_records_count", &Options::max_records_count)
                    .def_readwrite("clear_on_stop", &Options::clear_on_stop);
            }

            {
                constexpr auto doc_class = "TODO: DOCUMENTATION HERE.";

                py::class_<logging::LogHandler_History>(module, "LogHandler_History", doc_class)
                    .def(
                        py::init([](Options options)
                                 { return logging::LogHandler_History{ std::move(options) }; }),
                        py::arg("options") = Options{},
                        py::return_value_policy::move
                    )
                    .def("capture_history", &logging::LogHandler_History::capture_history)
                    .def("clear_history", &logging::LogHandler_History::clear_history)
                    .def("is_started", &logging::LogHandler_History::is_started)
                    .def(
                        "get_options",
                        &logging::LogHandler_History::get_options,
                        py::return_value_policy::copy
                    );
            }

            register_cpp_loghandler_type_for_anyloghandler<logging::LogHandler_History>();
        }
    }

    void bind_submodule_logging(pybind11::module_ module)
    {
        namespace py = pybind11;

        {
            static constexpr auto doc_log_level = R"(Level of logging, used to filter out logs which are at a lower level than the current one.
    - see `libmambapy.logging.LoggingParams`
    - see `libmambapy.logging.LogRecord`
    - see `libmambapy.logging.set_log_level`)";
            py::native_enum<mamba::log_level>(module, "LogLevel", "enum.Enum", doc_log_level)
                .value("TRACE", mamba::log_level::trace)
                .value("DEBUG", mamba::log_level::debug)
                .value("INFO", mamba::log_level::info)
                .value("WARNING", mamba::log_level::warn)
                .value("ERROR", mamba::log_level::err)
                .value("CRITICAL", mamba::log_level::critical)
                .value("OFF", mamba::log_level::off)
                .value("ALL", mamba::log_level::all)
                .finalize();


            module.def(
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
            py::native_enum<mamba::log_source>(module, "LogSource", "enum.Enum", doc_log_source)
                .value("LIBMAMBA", mamba::log_source::libmamba)
                .value("LIBCURL", mamba::log_source::libcurl)
                .value("LIBSOLV", mamba::log_source::libsolv)
                .value("TESTS", mamba::log_source::tests)
                .value("EXTERNAL", mamba::log_source::external)
                .finalize();


            module.def(
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
            py::class_<logging::LoggingParams>(module, "LoggingParams", "Parameters for the logging system.")
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

            py::class_<logging::LogRecord>(module, "LogRecord", doc_logrecord)
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

        bind_any_log_handler(module);

        {
            // TODO: add documentation + args
            module.def("stop_logging", &logging::stop_logging, py::return_value_policy::move);
            module.def(
                "set_log_handler",
                [](py::object handler,
                   std::optional<logging::LoggingParams> maybe_new_params,
                   std::vector<logging::log_source> new_log_sources) -> logging::AnyLogHandler
                {
                    return logging::set_log_handler(
                        to_anyloghandler(handler),
                        std::move(maybe_new_params),
                        std::move(new_log_sources)
                    );
                },
                py::return_value_policy::move
            );

            // WARNING:
            // Do not expose this function for now as we are not sure if it's useful and it might
            // cause UB easilly.
            // module.def("get_log_handler", &logging::get_log_handler,
            // py::return_value_policy::reference);

            module.def("set_log_level", &logging::set_log_level);
            module.def("get_log_level", &logging::get_log_level);
            module.def("get_logging_params", &logging::get_logging_params);
            module.def("set_logging_params", &logging::set_logging_params);
            module.def("log", &logging::log);
            module.def("enable_backtrace", &logging::enable_backtrace);
            module.def("disable_backtrace", &logging::disable_backtrace);
            module.def("log_backtrace", &logging::log_backtrace);
            module.def("log_backtrace_no_guards", &logging::log_backtrace_no_guards);
            module.def("flush_logs", &logging::flush_logs);
            module.def("set_flush_threshold", &logging::set_flush_threshold);
        }
    }
}