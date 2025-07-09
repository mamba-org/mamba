// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_LOGGING_HPP
#define MAMBA_CORE_LOGGING_HPP

#include <array>
#include <concepts>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>

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

    auto name_of(log_source source) -> const char*;


    /// @returns All `log_source` values as a range.
    auto all_log_sources() -> std::initializer_list<log_source>;

    namespace logging
    {
        // FIXME: this is a placeholder, replace it by the real thing once available
        template <typename Interface, std::size_t local_storage_size = sizeof(std::shared_ptr<Interface>)>
        using SBO_storage = std::unique_ptr<Interface>;

        struct LogRecord
        {
            std::string message;
            log_level level;
            log_source source;
        };


        // NOTE: it might make more sense to talka bout sinks than sources when it comes to the
        // implementation

        template <class T>
        concept LogHandler = std::movable<T>
                             and requires(
                                 T& handler,
                                 const T& const_handler,
                                 LoggingParams params,
                                 std::vector<log_source> sources,
                                 LogRecord log_record,
                                 std::optional<log_source> source
                             )  // no value means all sources
        {
            // REQUIREMENT: all the following operations must be thread-safe

            //
            handler.start_log_handling(params, sources);

            //
            handler.stop_log_handling();

            //
            handler.set_log_level(params.logging_level);

            //
            handler.set_params(std::as_const(params));

            //
            handler.log(log_record);

            // log stacktrace in all sources
            handler.log_stacktrace();

            // log stacktrace only in specific source
            handler.log_stacktrace(source);

            // log stacktrace in all sources
            handler.log_stacktrace_no_guards();

            // log stacktrace only in specific source
            handler.log_stacktrace_no_guards(source);

            // flush all sources
            handler.flush();

            // flush only a specific source
            handler.flush(std::optional<log_source>{});
        };

        class AnyLogHandler
        {
        public:

            AnyLogHandler();
            ~AnyLogHandler();

            AnyLogHandler(AnyLogHandler&&);
            AnyLogHandler& operator=(AnyLogHandler&&);

            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>)
                        and LogHandler<T>  //
            AnyLogHandler(T&& handler);

            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>)
                        and LogHandler<T>  //
            AnyLogHandler(T* handler_ptr);

            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>)
                        and LogHandler<T>  //
            AnyLogHandler& operator=(T&& new_handler);

            template <class T>
                requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>)
                        and LogHandler<T>  //
            AnyLogHandler& operator=(T* new_handler_ptr);

            ///
            /// Pre-condition: `has_value() == true`
            auto start_log_handling(LoggingParams params, std::vector<log_source> sources) -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto stop_log_handling() -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto set_log_level(log_level new_level) -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto set_params(LoggingParams new_params) -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto log(LogRecord record) noexcept -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto log_stacktrace(std::optional<log_source> source = {}) noexcept -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto log_stacktrace_no_guards(std::optional<log_source> source = {}) noexcept -> void;

            ///
            /// Pre-condition: `has_value() == true`
            auto flush(std::optional<log_source> source = {}) noexcept -> void;

            /// @returns `true` if there is an handler object stored in `this`, `false` otherwise.
            auto has_value() const noexcept -> bool;

            /// @returns @see `has_value()`
            explicit operator bool() const noexcept
            {
                return has_value();
            }

            /// @returns An identifier for the stored object's type, or `std::nullopt` if there is
            ///          no stored object (`has_value() == false`).
            std::optional<std::type_index> type_id() const;

        private:

            struct Interface;

            template <LogHandler T>
            struct OwningImpl;

            template <LogHandler T>
            struct NonOwningImpl;

            SBO_storage<Interface> m_stored;
        };

        static_assert(LogHandler<AnyLogHandler>);  // NEEDED? AnyLogHandler must not recursively
                                                   // host itself

        // not thread-safe
        auto
        set_log_handler(AnyLogHandler handler, std::optional<LoggingParams> maybe_new_params = {})
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace mamba::logging
{
    // NOTE: the following definitions are inline for performance reasons.

    inline auto name_of(log_source source) -> const char*
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

    inline auto all_log_sources() -> std::initializer_list<log_source>
    {
        return { log_source::libmamba, log_source::libcurl, log_source::libsolv };
    }

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

    //////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////


    struct AnyLogHandler::Interface
    {
        virtual ~Interface() = default;

        virtual void start_log_handling(LoggingParams params, std::vector<log_source> sources) = 0;
        virtual void stop_log_handling() = 0;
        virtual void set_log_level(log_level new_level) = 0;
        virtual void set_params(LoggingParams new_params) = 0;
        virtual void log(LogRecord record) noexcept = 0;
        virtual void log_stacktrace(std::optional<log_source> source = {}) noexcept = 0;
        virtual void log_stacktrace_no_guards(std::optional<log_source> source = {}) noexcept = 0;
        virtual void flush(std::optional<log_source> source = {}) noexcept = 0;
        virtual std::optional<std::type_index> type_id() const = 0;
    };

    template <LogHandler T>
    struct AnyLogHandler::OwningImpl : Interface
    {
        T object;
        void start_log_handling(LoggingParams params, std::vector<log_source> sources) override;
        void stop_log_handling() override;
        void set_log_level(log_level new_level) override;
        void set_params(LoggingParams new_params) override;
        void log(LogRecord record) noexcept override;
        void log_stacktrace(std::optional<log_source> source = {}) noexcept override;
        void log_stacktrace_no_guards(std::optional<log_source> source = {}) noexcept override;
        void flush(std::optional<log_source> source = {}) noexcept override;
        std::optional<std::type_index> type_id() const override;
    };

    template <LogHandler T>
    struct AnyLogHandler::NonOwningImpl : Interface
    {
        T* object;
        void start_log_handling(LoggingParams params, std::vector<log_source> sources) override;
        void stop_log_handling() override;
        void set_log_level(log_level new_level) override;
        void set_params(LoggingParams new_params) override;
        void log(LogRecord record) noexcept override;
        void log_stacktrace(std::optional<log_source> source = {}) noexcept override;
        void log_stacktrace_no_guards(std::optional<log_source> source = {}) noexcept override;
        void flush(std::optional<log_source> source = {}) noexcept override;
        std::optional<std::type_index> type_id() const override;
    };

}

#endif
