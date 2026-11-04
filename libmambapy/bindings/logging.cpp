// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <pybind11/native_enum.h>
#include <pybind11/stl.h>

#include "mamba/core/logging.hpp"
#include "mamba/core/logging_tools.hpp"

#include "logging.hpp"

namespace mambapy
{
    namespace logging = mamba::logging;

    using InvalidInterfaceReport = std::vector<std::string>;

    static constexpr auto doc_check_log_handler_interface =
        R"(Checks that the provided object's interface matches the expected interface for a `LogHandler`.
Returns a list of errors as string specifying missing member functions. If the list is empty
there is no error and the type matches the expected interface (or at least the member names).
)

  early_exit : if `True`, the function will return after finding the first error.
)";

    auto check_log_handler_interface(const pybind11::object& object, bool early_exit)
        -> std::optional<InvalidInterfaceReport>
    {
        using namespace std::literals;

        std::optional<InvalidInterfaceReport> result;

        const auto add_failure = [&](std::string failure_msg)
        {
            if (not result)
            {
                result.emplace();
            }
            result->push_back(failure_msg);
        };

        struct FuncDesc
        {
            const char* name;
            const char* signature;
        };

        static constexpr std::array expected_member_descs{
            FuncDesc{ "start_log_handling", "(params: LogParams, sources: [LogSource])" },
            FuncDesc{ "stop_log_handling", "()" },
            FuncDesc{ "set_log_level", "(log_level: LogLevel)" },
            FuncDesc{ "set_params", "(params: LogParams)" },
            FuncDesc{ "log", "(record: LogRecord)" },
            FuncDesc{ "enable_backtrace", "(size: int)" },
            FuncDesc{ "log_backtrace", "()" },
            FuncDesc{ "log_backtrace_no_guards", "()" },
            FuncDesc{ "flush", "(source: LogSource|None)" },
            FuncDesc{ "set_flush_threshold", "(level: LogLevel)" }
        };

        for (const auto& desc : expected_member_descs)
        {
            if (not pybind11::hasattr(object, desc.name))
            {
                add_failure(std::format("member not found: {}{}", desc.name, desc.signature));
                if (early_exit)
                {
                    break;
                }
            }
        }

        return result;
    }

    static constexpr auto doc_has_log_handler_interface =
        R"(Returns `True` if the provided object's interface satisfy the interface requirements for a `LogHandler`.
This is equivalent to `check_log_handler_interface(object, early_exit = true)`.
)";

    auto has_log_handler_interface(const pybind11::object& object) -> bool
    {
        return not check_log_handler_interface(object, true).has_value();
    }

    static constexpr auto doc_validate_log_handler_interface = R"(Throws an exception with detailed message if `check_log_handler_interface(pyobject, early_exit=true)` returns an error.)
    Mainly used to make sure an object has the complete expected interface for a `LogHandler`.)";

    auto validate_log_handler_interface(const pybind11::object& object) -> void
    {
        auto maybe_errors = check_log_handler_interface(object, false);
        if (maybe_errors)
        {
            throw std::invalid_argument(
                fmt::format(
                    "object does not have a LogHandler interface : {}",
                    std::move(maybe_errors).value()
                )
            );
        }
    }

    namespace
    {
        // THINK: should this be atomic, knowing only python code will change it?
        bool is_log_handler_interface_validation_enabled = false;
    }

    static constexpr auto doc_enable_log_handler_interface_validation =
        R"(Enables or disable automatic interface runtime validate of python objects wrapped into `AnyLogHandler`.
If enabled, will call `validate_log_handler_interface(pyobject)` every time a python object is passed to
`libmambapy.logging.set_log_handler()` or when an `AnyLogHandler` is constructed.
In this case, if the object doesn'have the complete expected interface, an exception will be raised.
If disabled, missing functions will be found only when called and could be never checked.

Enabling this is mainly useful in tests and debugging and is disabled by default.

Returns the previous value.)";

    auto enable_log_handler_interface_validation(bool enabled) -> bool
    {
        return std::exchange(is_log_handler_interface_validation_enabled, enabled);
    }

    namespace details
    {
        namespace
        {
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

            using AnyLogHandlerConstructorRegistry = std::
                unordered_map<pybind11::type, AnyLogHandlerConstructor, pytype_hash, pytype_equal>;

            AnyLogHandlerConstructorRegistry registry_anyloghandler_constructors;

        }

        auto
        register_anyloghandler_constructor(pybind11::type type_id, AnyLogHandlerConstructor constructor)
            -> void
        {
            registry_anyloghandler_constructors.emplace(type_id, constructor);
        }

        // note: we take `pyobject` by reference to be able to conditionally move it
        auto make_anyloghandler_from_cpp_loghandler_type(pybind11::object& pyobject)
            -> std::optional<mamba::logging::AnyLogHandler>
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
        // Wraps a Python object that provides `LogHandler` interface into a C++ type which can be
        // passed to `AnyLogHandler`.
        // It satisfy the `LogHandler` concept and will forward to the python object the calls
        // to a `LogHandler` interface.
        // This also helps identify objects that were defined in Python and not C++.
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
                if (is_log_handler_interface_validation_enabled)
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
                impl.attr("stop_log_handling")();
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
                impl.attr("log_backtrace")();
            }

            auto log_backtrace_no_guards() -> void
            {
                impl.attr("log_backtrace_no_guards")();
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

        // Returns `true` if the provided AnyLogHandler contains a value even if it was defined in
        // Python, `false` otherwise.
        auto has_valid_object(const logging::AnyLogHandler& handler) -> bool
        {
            const auto stored_type_id = handler.type_id();
            if (stored_type_id == typeid(PyAnyLogHandler))
            {
                return handler.unsafe_get<PyAnyLogHandler>()->is_valid();
            }
            return stored_type_id.has_value();
        }

        // Returns `true`  if the provided AnyLogHandler contains a value that was defined in Python
        // even if it's `None`, `false` otherwise.
        auto has_python_object(const logging::AnyLogHandler& handler) -> bool
        {
            auto maybe_type_id = handler.type_id();
            return maybe_type_id and maybe_type_id.value() == typeid(PyAnyLogHandler);
        }

        // Returns a python object if the provided AnyLogHandler contains one that was defined in
        // Python, nullopt otherwise.
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

        // Constructs an `AnyLogHandler` from a python object.
        // If the python object is wrapping a binded C++ `LogHandler` type,
        // we also attempt to extract the C++ type to store it into the `AnyLogHandler` returned.
        // This is to avoid ending up with a python object wrapping an `AnyLogHandler` wrapping
        // a python object wrapping a C++ `LogHandler` implementation. Instead we end up
        // with an `AnyLogHandler` wrapping directly the C++ implementation.
        // If the object was defined in Python, we wrap it (using `PyAnyLogHandler`)
        // so that it satisfies the compile-time requirements of `AnyLogHandler` and
        // forward calls from the logging system to python implementation.
        auto to_anyloghandler(pybind11::object pyobject) -> logging::AnyLogHandler
        {
            // When the object passed is actually a C++ `logging::AnyLogHandler`, we take it by
            // move. When it's any other object, we assume it's a python log handler and wrap it
            // appropriately.


            namespace py = pybind11;
            if (pyobject.is_none())
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

            static constexpr auto doc_class =
                R"(Stores or refers to a log handler implementation object which must satisfy the
requirements from `mamba::logging::LogHandler` (see `check_log_handler_interface()`).

Used in the logging system functions to register a log handler implementation.
(This is an API barrier, separating the logging system implementation from the log
handler implementation, whatever it is).

This wrapper can contain either C++ or Python log handler implementations.

Only operations specified as such are thread-safe.)";

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

            module.def(
                "check_log_handler_interface",
                &check_log_handler_interface,
                doc_check_log_handler_interface
            );
            module.def(
                "has_log_handler_interface",
                &has_log_handler_interface,
                doc_has_log_handler_interface
            );
            module.def(
                "validate_log_handler_interface",
                &validate_log_handler_interface,
                doc_validate_log_handler_interface
            );
            module.def(
                "enable_log_handler_interface_validation",
                &enable_log_handler_interface_validation,
                doc_enable_log_handler_interface_validation
            );
        }
    }

    // Translates a python frame object (see `inspect.currentframe()`) into a C++ source location
    // value. If the object is `None`, we use `inspect.currentframe()` instead.
    auto py_frame_to_source_location(pybind11::object frame_like_object) -> logging::SourceLocation
    {
        auto inspect_module = pybind11::module_::import("inspect");
        if (frame_like_object.is_none())
        {
            frame_like_object = inspect_module.attr("currentframe")();
        }

        assert(not frame_like_object.is_none());

        if (pybind11::isinstance<logging::SourceLocation>(frame_like_object))
        {
            return frame_like_object.cast<logging::SourceLocation>();
        }

        auto frame_info = inspect_module.attr("getframeinfo")(frame_like_object);

        auto positions = frame_info.attr("positions");
        return logging::SourceLocation{ frame_info.attr("filename").cast<std::string>(),
                                        frame_info.attr("function").cast<std::string>(),
                                        positions.attr("lineno").cast<std::uint_least32_t>(),
                                        positions.attr("col_offset").cast<std::uint_least32_t>() };
    }

    void bind_submodule_logging(pybind11::module_ module)
    {
        namespace py = pybind11;

        {
            static constexpr auto doc_log_level = R"(Level of logging, used to filter out logs which are at a lower level than the current one.
    - see `libmambapy.logging.LoggingParams`
    - see `libmambapy.logging.LogRecord`
    - see `libmambapy.logging.set_log_level`)";
            py::native_enum<mamba::log_level>(module, "LogLevel", "enum.IntEnum", doc_log_level)
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
                    // runtime address until instantiated here.
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
                    // runtime address until instantiated here.
                    return mamba::name_of(value);
                },
                "Provides the name that will be used in `LogRecord`s for the specified LogLevel.",
                py::arg("log_level")
            );
        }

        {
            static constexpr auto doc_loggingparams_logging_level = "Minimum level a log record must have to not be filtered out.";
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
            static constexpr auto doc_type = R"(MISSING DOC HERE`)";

            py::class_<logging::SourceLocation>(module, "SourceLocation", doc_type)
                .def(py::init(&py_frame_to_source_location), py::arg("frame") = py::none{})
                .def_property_readonly(
                    "filename",
                    [](const logging::SourceLocation& self) { return self.file_name(); }
                )
                .def_property_readonly(
                    "function",
                    [](const logging::SourceLocation& self) { return self.function_name(); }
                )
                .def_property_readonly(
                    "lineno",
                    [](const logging::SourceLocation& self) { return self.line(); }
                )
                // this member difers from `inspect.FrameInfo` interface but matches
                // `std::source_location`
                .def_property_readonly(
                    "column",
                    [](const logging::SourceLocation& self) { return self.file_name(); }
                );

            py::implicitly_convertible<py::object, logging::SourceLocation>();
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
                    ),
                    py::arg("message"),
                    py::arg("level"),
                    py::arg("source") = logging::log_source::external,
                    py::arg("location") = logging::SourceLocation{}

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
            static constexpr auto doc_stop_logging =
R"(Stops the logging system.

Unregisters the current log handler if any is registered.
This is equivalent to `libmamba.logging.set_log_handler(None)`,
@see `set_log_handler` for  details.

This call is NOT thread-safe.

Returns An `AnyLogHandler` containing the registered log handler if any.)";

            static constexpr auto doc_set_log_handler =
R"(Registers a log handler to use in the logging system, or `None` to stop the logging system.

The other logging operations will be no-op if no log handler is registered.

If a log handler is registered before this call, this function call
`stop_logging` with the same arguments. If a log handler object is
provided as argument, this function will call this handler's
`start_logging` function with the same `LoggingParams` as argument.

This call is NOT thread-safe.

Parameters:
- handler: The log handler implementation to use when the other operations of the
    logging system are called, or `None` to stop the logging system.
- maybe_new_params: Optional new parameters to the logging system. If provided,
    overwrites the current parameters. If not, the previous parameters
    will be used.
- new_log_sources: List of possible `LogRecords` origins used by libmamba. This is
    mostly used in implementations that need to setup separate log
    sinks depending on the source.

Returns An `libmamba.logging.AnyLogHandler` containing the previously registered log handler if any.
)";


            static constexpr auto doc_set_log_level =
R"(Changes the logging system logging level.

After this call, if a log handler is registered and `libmamba.logging.log(log_record)` is called, the
provided log record will be ignored if it's log level is lesser than the current logging
system log level.

If a log handler is registered, this function call it's `set_log_level` with
the same arguments.

This call is thread safe as long as the log handler implementation fulfills the
thread-safety requirements.

Returns The previous log level of the logging system.
)";

            static constexpr auto doc_get_log_level =
R"(Returns the current log level of the logging system.
This call is thread-safe but the returned value must be considered immediately obsolete.)";

            static constexpr auto doc_get_logging_params =
R"(Returns the current configuration of the logging system.
This call is thread-safe but the returned value must be considered immediately obsolete.)";

            static constexpr auto doc_set_logging_params =
R"(Changes the logging system configuration.

If a log handler is registered and the parameter `update_log_handler` is `true`,
this function calls it's `set_params` member with the same arguments.

This call is thread safe as long as the log handler implementation fulfills the
thread-safety requirements.

Warning: `update_log_handler = false` should only be used when implementing
other operations that need to update the params and then update the log handler
themselves, separately.

Returns the previous configuration of the logging system.)";

            static constexpr auto doc_log =
R"(Process a log record to be sent in a logging sink to be printed if the necessary conditions are met.

If a log handler is registered, this function calls it's `log` member with the same
arguments, otherwise this function will do nothing.

The implementation must ignore log records with a log level lower then the current
logging system logging level, see `libmamba.logging.get_log_level`.

If the log record is not ignored, it must be either sent in a logging sink, or
if backtrace is enabled, it must be pushed in the backtrace history,
see `enable_backtrace` for details.

The implementation is free to flush or not it's internal sinks when the provided log
records has a lower logging level than the current flush threshold.
However if the log record has a level greater or equal than the current flush threshold,
the implementation must flush the sink related to the log record's source.

This call is thread safe as long as the log handler implementation fulfills the
thread-safety requirements.)";

            static constexpr auto doc_enable_backtrace =
R"(Enabling, configuring or disabling the backtrace functionality.

If a log handler is registered, this function calls it's `enable_backtrace` member
with the same arguments, otherwise this function will do nothing.

A specified size of zero means disabling the backtrace feature.
Any other values means the backtrace functionality is enabled.

If the feature is enabled, log records which are not filtered out by their log levels
must be kept in order in an history buffer of the specified size, see
`log`.

The implementation shall keep track of only the last log records pushed in the
backtrace. If the history size is already equal to the specified size and
a new log should be pushed in the history, the implementation must remove the oldest
log record and push the new log record in the history.

Log records in the backtrace must only be sent to logging sinks once either
`libmamba.logging.log_backtrace` or `libmamba.logging.log_backtrace_no_guards` is called, see
`libmamba.logging.log_backtrace`.

This call is thread safe as long as the log handler implementation fulfills the
thread-safety requirements.

Parameters:
- records_buffer_size:  Size of the history buffer that will contain the log records.
    A size of zero means disabling the backtrace feature.
    Any other values means the backtrace functionality is enabled.)";

            static constexpr auto doc_disable_backtrace = R"(Equivalent to `libmamba.logging.enable_backtrace(0)`.)";

            static constexpr auto doc_log_backtrace =
R"(Sends the log records in the backtrace history to the implementation's logging sinks.

If a log handler is registered, this function calls it's `log_backtrace` member,
otherwise this function will do nothing.

After this call, the backtrace history shall be empty.

This call is thread safe as long as the log handler implementation fulfills the
thread-safety requirements.)";

            static constexpr auto doc_log_backtrace_no_guards =
R"(Sends the log records in the backtrace history to the implementation's logging sinks,
but with filtering the logging level of the log records as if they were logged now.

If a log handler is registered, this function calls
it's `log_backtrace_no_guards` member, otherwise this function will do nothing.

After this call, the backtrace history shall be empty.

This call is thread safe as long as the log handler implementation fulfills the
thread-safety requirements.)";

            static constexpr auto doc_flush_logs =
R"(Flushes all the logging sinks.

If a log handler is registered, this function calls it's `flush` member with the same
argument, otherwise this function will do nothing.

The implementation is also free to flush at any other time but must guarantee
the flush is done after then end of this operation.

This call is thread safe as long as the log handler implementation fulfills the
thread-safety requirements.)";

            static constexpr auto doc_set_flush_threshold =
R"(Set a flush threshold, a logging level for which log records pushed in `log` will
trigger a flush of logging sinks.

If a log handler is registered, this function calls it's `set_flush_threshold` member
with the same argument, otherwise this function will do nothing.

The flush threshold is the logging level for which if a log record is pushed
and has this logging level or higher, the implementation must flush it's sinks
immediately.

If the specified level is "all", the implementation must flush at every call to
`libmamba.logging.log`.

The implementation is also free to flush at any other time but must guarantee
the flush is done after then end of this operation.

This call is thread safe as long as the log handler implementation fulfills the
thread-safety requirements.

See `libmamba.logging.log`.)";

            module.def("stop_logging", &logging::stop_logging, doc_stop_logging, py::return_value_policy::move);
            module.def(
                "set_log_handler",
                [](py::object handler,
                   std::optional<logging::LoggingParams> maybe_new_params,
                   std::optional<std::vector<logging::log_source>> new_log_sources) -> logging::AnyLogHandler
                {
                    return logging::set_log_handler(
                        to_anyloghandler(handler),
                        std::move(maybe_new_params),
                        new_log_sources ? std::move(*new_log_sources)
                                        : std::vector<logging::log_source>{}
                    );
                },
                py::arg("log_handler"),
                py::arg("params") = std::nullopt,
                py::arg("sources") = std::nullopt,
                doc_set_log_handler,
                py::return_value_policy::move
            );

            // WARNING:
            // Do not expose this function for now as we are not sure if it's useful and it might
            // cause UB easily.
            // module.def("get_log_handler", &logging::get_log_handler, py::return_value_policy::reference);

            module.def("set_log_level", &logging::set_log_level, doc_set_log_level);
            module.def("get_log_level", &logging::get_log_level, doc_get_log_level);
            module.def("get_logging_params", &logging::get_logging_params, doc_get_logging_params);
            module.def("set_logging_params", &logging::set_logging_params, doc_set_logging_params);
            module.def("log", &logging::log, doc_log);
            module.def("enable_backtrace", &logging::enable_backtrace, doc_enable_backtrace);
            module.def("disable_backtrace", &logging::disable_backtrace, doc_disable_backtrace);
            module.def("log_backtrace", &logging::log_backtrace, doc_log_backtrace);
            module.def("log_backtrace_no_guards", &logging::log_backtrace_no_guards, doc_log_backtrace_no_guards);
            module.def("flush_logs", &logging::flush_logs, doc_flush_logs);
            module.def("set_flush_threshold", &logging::set_flush_threshold, doc_set_flush_threshold);
        }
    }
}
