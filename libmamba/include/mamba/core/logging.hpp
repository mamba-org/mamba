// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_LOGGING_HPP
#define MAMBA_CORE_LOGGING_HPP

#include <array>
#include <cassert>
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
        std::string_view log_pattern{ "%^%-9!l%-8n%$ %v" };  // IS THIS SPECIFIC TO spdlog???
        std::size_t log_backtrace{ 0 };
    };

    enum class log_source  // "source" isnt the best way to put it, maybe channel? component? sink?
    {
        libmamba,
        libcurl,
        libsolv
    };

    /// @returns The name of the specified log source as an UTF-8 null-terminated string.
    constexpr auto name_of(log_source source) -> const char*;


    /// @returns All `log_source` values as a range.
    constexpr auto all_log_sources() -> std::initializer_list<log_source>;

    namespace logging
    {
        // FIXME: this is a placeholder, replace it by the real thing once available
        template <typename Interface, std::size_t local_storage_size = sizeof(std::shared_ptr<Interface>)>
        using SBO_storage = std::unique_ptr<Interface>;

        struct LogRecord
        {
            std::string message; // THINK: cool be made lazy if it was a function instead
            log_level level;
            log_source source;
        };


        // NOTE: it might make more sense to talka bout sinks than sources when it comes to the
        // implementation

        template <typename T>
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

        template <typename T>
        concept LogHandlerOrPtr = LogHandler<T> or (std::is_pointer_v<T> and LogHandler<std::remove_pointer_t<T>>);

        class AnyLogHandler
        {
        public:

            constexpr AnyLogHandler() = default;
            constexpr ~AnyLogHandler() = default;

            AnyLogHandler(AnyLogHandler&&) noexcept = default;
            AnyLogHandler& operator=(AnyLogHandler&&) noexcept = default;

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
            auto type_id() const noexcept -> std::optional<std::type_index>;

        private:

            struct Interface;

            template <LogHandlerOrPtr T>
            struct Wrapper;

            SBO_storage<Interface> m_storage;
        };

        static_assert(LogHandler<AnyLogHandler>);  // NEEDED? AnyLogHandler must not recursively
                                                   // host itself

        ////////////////////////////////////////////////////////////////////////////////
        // Logging System API


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

    inline constexpr auto name_of(log_source source) -> const char*
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
        return "unknown";
    }

    inline constexpr auto all_log_sources() -> std::initializer_list<log_source>
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
        virtual void log_stacktrace(std::optional<log_source> source) noexcept = 0;
        virtual void log_stacktrace_no_guards(std::optional<log_source> source) noexcept = 0;
        virtual void flush(std::optional<log_source> source) noexcept = 0;
        virtual std::type_index type_id() const = 0;
    };

    template <LogHandler T>
    T& as_ref(T& object)
    {
        return object;
    }

    template <LogHandler T>
    T& as_ref(T* object)
    {
        assert(object);
        return *object;
    }

    template <LogHandlerOrPtr T>
    struct AnyLogHandler::Wrapper : Interface
    {
        T object;

        Wrapper(T new_object)
            : object(std::move(new_object))
        {
        }

        void start_log_handling(LoggingParams params, std::vector<log_source> sources) override
        {
            as_ref(object).start_log_handling(std::move(params), std::move(sources));
        }

        void stop_log_handling() override
        {
            as_ref(object).stop_log_handling();
        }

        void set_log_level(log_level new_level) override
        {
            as_ref(object).set_log_level(new_level);
        }

        void set_params(LoggingParams new_params) override
        {
            as_ref(object).set_params(std::move(new_params));
        }

        void log(LogRecord record) noexcept override
        {
            as_ref(object).log(std::move(record));
        }

        void log_stacktrace(std::optional<log_source> source) noexcept override
        {
            as_ref(object).log_stacktrace(std::move(source));
        }

        void log_stacktrace_no_guards(std::optional<log_source> source) noexcept override
        {
            as_ref(object).log_stacktrace_no_guards(std::move(source));
        }

        void flush(std::optional<log_source> source) noexcept override
        {
            as_ref(object).flush(std::move(source));
        }

        std::type_index type_id() const override
        {
            return typeid(object);
        }
    };

    template <class T>
        requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>) and LogHandler<T>
    AnyLogHandler::AnyLogHandler(T&& handler)
        : m_storage(std::make_unique<Wrapper<T>>(std::forward<T>(handler)))
    {

    }

    template <class T>
        requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>) and LogHandler<T>
    AnyLogHandler::AnyLogHandler(T* handler_ptr)
        : m_storage(std::make_unique<Wrapper<T*>>(handler_ptr))
    {
        assert(handler_ptr);
    }

    template <class T>
        requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>) and LogHandler<T>
    AnyLogHandler& AnyLogHandler::operator=(T&& new_handler)
    {
        if (m_storage and typeid(T) == m_storage->type_id())
        {
            static_cast<Wrapper<T>*>(m_storage.get())->object = std::forward<T>(new_handler);
        }
        else
        {
            m_storage = std::make_unique<Wrapper<T>>(std::forward<T>(new_handler));
        }
        return *this;
    }

    template <class T>
        requires(not std::is_same_v<std::remove_cvref_t<T>, AnyLogHandler>) and LogHandler<T>
    AnyLogHandler& AnyLogHandler::operator=(T* new_handler_ptr)
    {
        assert(new_handler_ptr);
        if (m_storage and typeid(T*) == m_storage->type_id())
        {
            static_cast<Wrapper<T>*>(m_storage.get())->object = new_handler_ptr;
        }
        else
        {
            m_storage = std::make_unique<Wrapper<T*>>(new_handler_ptr);
        }
        return *this;
    }

    inline auto AnyLogHandler::start_log_handling(LoggingParams params, std::vector<log_source> sources) -> void
    {
        assert(m_storage);
        m_storage->start_log_handling(std::move(params), std::move(sources));
    }

    inline auto AnyLogHandler::stop_log_handling() -> void
    {
        assert(m_storage);
        m_storage->stop_log_handling();
    }

    inline auto AnyLogHandler::set_log_level(log_level new_level) -> void
    {
        assert(m_storage);
        m_storage->set_log_level(new_level);
    }

    inline auto AnyLogHandler::set_params(LoggingParams new_params) -> void
    {
        assert(m_storage);
        m_storage->set_params(new_params);
    }

    inline auto AnyLogHandler::log(LogRecord record) noexcept -> void
    {
        assert(m_storage);
        m_storage->log(std::move(record));
    }

    inline auto AnyLogHandler::log_stacktrace(std::optional<log_source> source) noexcept -> void
    {
        assert(m_storage);
        m_storage->log_stacktrace_no_guards(std::move(source));
    }

    inline auto
        AnyLogHandler::log_stacktrace_no_guards(std::optional<log_source> source) noexcept
        -> void
    {
        assert(m_storage);
        m_storage->log_stacktrace_no_guards(std::move(source));
    }

    inline auto AnyLogHandler::flush(std::optional<log_source> source) noexcept -> void
    {
        assert(m_storage);
        m_storage->flush(std::move(source));
    }

    inline auto AnyLogHandler::has_value() const noexcept -> bool
    {
        return m_storage ? true : false;
    }

    inline auto AnyLogHandler::type_id() const noexcept -> std::optional<std::type_index>
    {
        return m_storage ? std::make_optional(m_storage->type_id()) : std::nullopt;
    }

}

#endif
