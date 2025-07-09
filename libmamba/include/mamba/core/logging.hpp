// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_LOGGING_HPP
#define MAMBA_CORE_LOGGING_HPP

#include <concepts>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <typeindex>

namespace mamba
{

    enum class log_level
    {
        trace,
        debug,
        info,
        warn,
        err,
        critical,
        off
    };

    struct LoggingParams
    {
        int verbosity{ 0 };
        log_level logging_level{ log_level::warn };
        std::string log_pattern{ "%^%-9!l%-8n%$ %v" };  // IS THIS SPECIFIC TO spdlog???
        std::size_t log_backtrace{ 0 };
    };

    enum class log_source  // "source" isnt the best way to put it, maybe channel? component? sink?
    {
        libmamba,
        libcurl,
        libsolv
    };

    inline auto name_of(log_source source)
    {
        switch (source)
        {
            case log_source::libmamba:
                return "libmamba";
            case log_source::libcurl:
                return "libcurl";
            case log_source::libsolv:
                return "libsolv";
        }

        // TODO(c++23): std::unreachable();
        return "";
    }

    inline auto all_log_sources() -> std::vector<log_source>  // TODO: do better :|
    {
        return { log_source::libmamba, log_source::libcurl, log_source::libsolv };
    }

    namespace logging
    {
        struct LogRecord
        {
            std::string message;
            log_level level;
            log_source source;
        };


        // NOTE: it might make more sense to talka bout sinks than sources when it comes to the
        // implementation

        template <class T>
        concept LogHandler = std::movable<T> // at a minimum it must be movable
                             and std::equality_comparable<T>  // and comparable TODO: is this really necessary?
                             and requires(
                                 T& handler,
                                 const T& const_handler  //
                                 ,
                                 LoggingParams params,
                                 std::vector<log_source> sources,
                                 LogRecord log_record  //
                                 ,
                                 std::optional<log_source> source  // no value means all sources
                             ) {
                                    handler.start_log_handling(params, sources);  //
                                    handler.stop_log_handling();                  //
                                    handler.set_log_level(params.logging_level);  //
                                    handler.set_params(std::as_const(params));    //
                                    handler.log(log_record);                      //
                                    handler.log_stacktrace();  // log stacktrace in all sources
                                    handler.log_stacktrace(source);      // log stacktrace only in
                                                                         // specific source
                                    handler.log_stacktrace_no_guards();  // log stacktrace in all
                                                                         // sources
                                    handler.log_stacktrace_no_guards(source);    // log stacktrace
                                                                                 // only in specific
                                                                                 // source
                                    handler.flush();                             // flush all
                                    handler.flush(std::optional<log_source>{});  // flush only a
                                                                                 // specific source
                                };

        class AnyLogHandler
        {
        public:

            AnyLogHandler();
            ~AnyLogHandler();

            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>) and LogHandler<T>
            AnyLogHandler(T&& handler);

            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>) and LogHandler<T>
            AnyLogHandler(T* handler_ptr);

            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>) and LogHandler<T>
            AnyLogHandler& operator=(T&& new_handler);

            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>) and LogHandler<T>
            AnyLogHandler& operator=(T* new_handler_ptr);

            auto start_log_handling(LoggingParams params, std::vector<log_source> sources) -> void;
            auto stop_log_handling() -> void;

            auto set_log_level(log_level new_level) -> void;
            auto set_params(LoggingParams new_params) -> void;

            auto log(LogRecord record) noexcept -> void;

            auto log_stacktrace(std::optional<log_source> source = {}) noexcept -> void;
            auto log_stacktrace_no_guards(std::optional<log_source> source = {}) noexcept -> void;
            auto flush(std::optional<log_source> source = {}) noexcept -> void;

            auto operator==(const AnyLogHandler& other) const noexcept -> bool;

            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>) and LogHandler<T>
            auto operator==(const T& other) const noexcept -> bool;

            auto has_value() const noexcept -> bool;

            explicit operator bool() const noexcept
            {
                return has_value();
            }

            /// @returns An identifier for the stored object's type, or `std::nullopt` if there is no
            ///          stored object.
            std::optional<std::type_index> type_id() const;

        private:

            struct Interface
            {
                virtual ~Interface() = 0;
            };
        };

        static_assert(LogHandler<AnyLogHandler>);  // NEEDED? AnyLogHandler must not recursively
                                                   // host itself

        // not thread-safe
        auto set_log_handler(AnyLogHandler handler, std::optional<LoggingParams> maybe_params = {})
            -> AnyLogHandler;

        // not thread-safe
        auto get_log_handler() -> AnyLogHandler&;

        // thread-safe
        auto set_log_level(log_level new_level) -> log_level;

        // thread-safe, value immediately obsolete
        auto get_log_level() noexcept -> log_level;

        // thread-safe, value immediately obsolete
        auto get_logging_params() noexcept -> LoggingParams;

        // thread-safe
        auto set_logging_params(LoggingParams new_params);

        // as thread-safe as handler's implementation if set
        auto log(LogRecord record) noexcept -> void;

        // as thread-safe as handler's implementation if set
        auto log_stacktrace(std::optional<log_source> source = {}) noexcept -> void;

        // as thread-safe as handler's implementation if set
        auto flush_logs(std::optional<log_source> source = {}) noexcept -> void;

        // as thread-safe as handler's implementation if set
        auto log_stacktrace_no_guards(std::optional<log_source> source = {}) noexcept -> void;

        ///////////////////////////////////////////////////////
        // MIGT DISAPPEAR SOON
        class MessageLogger
        {
        public:

            MessageLogger(log_level level);
            ~MessageLogger();

            std::stringstream& stream()
            {
                // Keep this implementation inline for performance reasons.
                return m_stream;
            }

            static void activate_buffer();
            static void deactivate_buffer();
            static void print_buffer(std::ostream& ostream);

        private:

            log_level m_level;
            std::stringstream m_stream;

            static void emit(const std::string& msg, const log_level& level);
        };

        //////////////////////////////////////////////////////////////////
        //////////////////////////////////////////////////////////////////

        // NOTE: the following definitions are inline to help with performance.

        // TODO: find a better name?
        template <typename Func, typename... Args>
           requires std::invocable<Func, AnyLogHandler&, Args...>
        auto call_log_handler_if_existing(Func&& func, Args&&... args) -> void
        {
            // TODO: consider enabling for user to specify that no check is needed (one less branch)
            if (auto& log_handler = get_log_handler())
            {
                std::invoke(std::forward<Func>(func), log_handler, std::forward<Args>(args)...);
            }
        }

        // as thread-safe as handler's implementation
        inline auto log(LogRecord record) noexcept -> void
        {
            call_log_handler_if_existing(&AnyLogHandler::log, std::move(record));
        }

        // as thread-safe as handler's implementation
        inline auto log_stacktrace(std::optional<log_source> source) noexcept -> void
        {
            call_log_handler_if_existing(&AnyLogHandler::log_stacktrace, std::move(source));
        }

        // as thread-safe as handler's implementation
        inline auto flush_logs(std::optional<log_source> source) noexcept -> void
        {
            call_log_handler_if_existing(&AnyLogHandler::flush, std::move(source));
        }

        // as thread-safe as handler's implementation
        inline auto log_stacktrace_no_guards(std::optional<log_source> source) noexcept -> void
        {
            call_log_handler_if_existing(&AnyLogHandler::log_stacktrace_no_guards, std::move(source));
        }


    }
}

#undef LOG
#undef LOG_TRACE
#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARNING
#undef LOG_ERROR
#undef LOG_CRITICAL

#define LOG(severity) mamba::logging::MessageLogger(severity).stream()
#define LOG_TRACE LOG(mamba::log_level::trace)
#define LOG_DEBUG LOG(mamba::log_level::debug)
#define LOG_INFO LOG(mamba::log_level::info)
#define LOG_WARNING LOG(mamba::log_level::warn)
#define LOG_ERROR LOG(mamba::log_level::err)
#define LOG_CRITICAL LOG(mamba::log_level::critical)


#endif
